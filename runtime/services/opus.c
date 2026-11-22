// hwopus: the Switch's hardware Opus decoder (IHardwareOpusDecoderManager / IHardwareOpusDecoder), on FFmpeg's Opus
// decoder (libavcodec, already linked for the movies: runtime/hle/movie.c).
//
// Pokémon Legends: Arceus decodes its background music through it: the logging stub answered GetWorkBufferSize with 0,
// the game never opened a decoder, and the music was silent while sound effects (PCM / ADPCM voices, audio.c) played
// (issue #17, Windows and Steam Deck alike). The game hands each call one packet - an 8-byte big-endian header (packet
// size, final range) and the packet - and gets interleaved PCM16 back at the rate it asked for. FFmpeg decodes at
// 48 kHz; the other rates the service takes (24 / 16 / 12 / 8 kHz) are whole fractions of it, decimated here through a
// windowed-sinc low-pass (fir_make) that carries its history across packets. Plain averaging (the first version)
// let everything above the new Nyquist fold back in: an audible hiss on the menus' 12 / 24 kHz streams (user test,
// 2026-11-16).
// LARECOMP_LOG_OPUS=1: every decode call.
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>

#include "../hle.h"
#include "../ipc.h"

#define OPUS_RESULT_INVALID MAKE_RESULT(111, 6)  // (codec module: a packet it can't decode)

#define FIR_MAX 129  // taps at the 8 kHz rate (16 per 48 kHz sample of decimation, +1)

typedef struct {
    AVCodecContext *ctx;
    AVPacket *pkt;
    AVFrame *frame;
    uint32_t rate, channels, down;  // down: 48 kHz / rate
    uint64_t decoded, failed;
    int taps;            // (0 at 48 kHz: copied as decoded)
    float fir[FIR_MAX];  // low-pass, cut at 0.45 x the output rate
    float *hist;         // per channel: the last taps - 1 input samples (48 kHz)
    float *x;            // scratch: history + this packet, one channel
    uint32_t xcap;
    int id;              // (logs: the n-th decoder opened)
    FILE *wav;           // LARECOMP_OPUS_WAV=<folder>: what this decoder hands the game, as a WAV
    uint32_t wav_bytes;
} OpusDec;

static void wav_hdr(FILE *f, uint32_t rate, uint32_t ch, uint32_t bytes) {
    uint32_t h[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746d66, 16, 0x0001u | (ch << 16), rate,
                      rate * 2 * ch, 0x00100000 | (2 * ch), 0x61746164, bytes};
    fseek(f, 0, SEEK_SET);
    fwrite(h, 4, 11, f);
    fseek(f, 0, SEEK_END);
}

static void fir_make(OpusDec *d) {
    if (d->down == 1) return;
    int t = 16 * (int)d->down + 1;
    if (t > FIR_MAX) t = FIR_MAX;
    double fc = 0.45 / d->down, sum = 0;  // (cycles per 48 kHz sample)
    for (int k = 0; k < t; k++) {
        double m = k - (t - 1) / 2.0, x = 2 * M_PI * fc * m;
        double sinc = m == 0 ? 2 * fc : sin(x) / (M_PI * m);
        double w = 0.42 - 0.5 * cos(2 * M_PI * k / (t - 1)) + 0.08 * cos(4 * M_PI * k / (t - 1));  // Blackman
        d->fir[k] = (float)(sinc * w), sum += sinc * w;
    }
    for (int k = 0; k < t; k++) d->fir[k] = (float)(d->fir[k] / sum);  // (unity gain at DC)
    d->taps = t;
    d->hist = calloc((size_t)d->channels * (size_t)(t - 1), sizeof *d->hist);
}

static int log_opus = -1;

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

// streams / coupled: the multistream layout (1 / channels - 1 for a plain mono or stereo stream); map: the channel
// mapping table (NULL: identity)
static OpusDec *dec_open(uint32_t rate, uint32_t channels, uint32_t streams, uint32_t coupled, const uint8_t *map) {
    if (!channels || channels > 255) return NULL;
    uint32_t down = rate == 48000 ? 1 : rate == 24000 ? 2 : rate == 16000 ? 3 : rate == 12000 ? 4 : rate == 8000 ? 6 : 0;
    if (!down) { LOG("[opus] unsupported sample rate %u\n", rate); return NULL; }
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec) { LOG("[opus] FFmpeg has no Opus decoder\n"); return NULL; }
    OpusDec *d = calloc(1, sizeof *d);
    d->rate = rate, d->channels = channels, d->down = down;
    fir_make(d);
    d->ctx = avcodec_alloc_context3(codec);
    d->ctx->sample_rate = 48000;
    av_channel_layout_default(&d->ctx->ch_layout, (int)channels);
    // OpusHead (RFC 7845): the channel count, no pre-skip (the hardware decoder skips nothing), and the stream layout -
    // mapping family 0 for one mono / stereo stream, 1 (up to 8 channels) or 255 otherwise
    bool family0 = channels <= 2 && streams == 1 && coupled == channels - 1;
    int size = family0 ? 19 : 21 + (int)channels;
    uint8_t *h = av_mallocz((size_t)size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(h, "OpusHead", 8);
    h[8] = 1, h[9] = (uint8_t)channels;
    h[12] = 0x80, h[13] = 0xBB;  // input rate 48000, little-endian (h[10..11] pre-skip 0, h[16..17] gain 0)
    h[18] = family0 ? 0 : channels <= 8 ? 1 : 255;
    if (!family0) {
        h[19] = (uint8_t)streams, h[20] = (uint8_t)coupled;
        for (uint32_t i = 0; i < channels; i++) h[21 + i] = map ? map[i] : (uint8_t)i;
    }
    d->ctx->extradata = h;
    d->ctx->extradata_size = size;
    if (avcodec_open2(d->ctx, codec, NULL) < 0) {
        LOG("[opus] cannot open a decoder (%u Hz, %u channels, %u streams)\n", rate, channels, streams);
        avcodec_free_context(&d->ctx);
        free(d->hist);
        free(d);
        return NULL;
    }
    d->pkt = av_packet_alloc();
    d->frame = av_frame_alloc();
    const char *wd = getenv("LARECOMP_OPUS_WAV");
    if (wd && *wd) {
        static int seq;
        char path[1024];
        snprintf(path, sizeof path, "%s/opus_%03d_%uHz_%uch.wav", wd, seq++, rate, channels);
        if ((d->wav = fopen(path, "wb"))) {
            wav_hdr(d->wav, rate, channels, 0);
            if (seq == 1) LOG("[opus] recording to %s\n", path);
        } else if (seq <= 3) {
            LOG("[opus] cannot write %s (%s)\n", path, strerror(errno));
        }
    }
    static int next_id;
    d->id = next_id++;
    LOG("[opus] decoder %d opened: %u Hz, %u channel(s), %u stream(s)\n", d->id, rate, channels, streams);
    return d;
}

static void dec_close(Service *s) {
    OpusDec *d = s->state;
    if (!d) return;
    LOG("[opus] decoder %d closed after %llu packets (%llu failed)\n", d->id, (unsigned long long)d->decoded, (unsigned long long)d->failed);
    if (d->wav) wav_hdr(d->wav, d->rate, d->channels, d->wav_bytes), fclose(d->wav);
    av_frame_free(&d->frame);
    av_packet_free(&d->pkt);
    avcodec_free_context(&d->ctx);
    free(d->hist);
    free(d->x);
    free(d);
    s->state = NULL;
}

// Rounded to nearest, as libopus's opus_decode (FLOAT2INT16) does on the hardware. Truncated (the first version),
// every sample was pulled toward zero: on the game's quiet streams (menu sounds peaking at -45 to -66 dBFS, a few
// steps of 16-bit) that is distortion the game's mixer then turns up - heard as hiss on the menus (2026-11-16).
static inline int16_t to_s16(float v) {
    int x = (int)lrintf(v * 32768.0f);
    return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
}

// One packet from in into out (interleaved PCM16 at the decoder's rate). Returns the result; *consumed / *samples
// (per channel) as the hardware reports them.
static uint32_t dec_packet(OpusDec *d, const IpcBuf *in, IpcBuf *out, bool reset, uint32_t *consumed, uint32_t *samples) {
    *consumed = *samples = 0;
    if (reset) {
        avcodec_flush_buffers(d->ctx);
        if (d->hist) memset(d->hist, 0, (size_t)d->channels * (size_t)(d->taps - 1) * sizeof *d->hist);
    }
    if (!in || in->size < 8) return OPUS_RESULT_INVALID;
    const uint8_t *src = (const uint8_t *)(uintptr_t)in->addr;
    uint32_t len = be32(src);
    if (len == 0 || len + 8 > in->size) return OPUS_RESULT_INVALID;
    if (av_new_packet(d->pkt, (int)len) < 0) return OPUS_RESULT_INVALID;
    memcpy(d->pkt->data, src + 8, len);
    int r = avcodec_send_packet(d->ctx, d->pkt);
    av_packet_unref(d->pkt);
    if (r < 0 || avcodec_receive_frame(d->ctx, d->frame) < 0) {
        if (d->failed++ < 5) LOG("[opus] a packet of %u bytes did not decode (%d)\n", len, r);
        *consumed = len + 8;  // (skip it: a stuck stream would ask for it again forever)
        return 0;
    }
    AVFrame *f = d->frame;
    uint32_t ch = d->channels, n48 = (uint32_t)f->nb_samples, n = n48 / d->down;
    int16_t *dst = out ? (int16_t *)(uintptr_t)out->addr : NULL;
    uint64_t cap = out ? out->size / (2ull * ch) : 0;
    if (n > cap) {
        if (d->failed++ < 5) LOG("[opus] output buffer holds %llu samples, the packet has %u: cut\n", (unsigned long long)cap, n);
        n = (uint32_t)cap;
    }
    bool planar = av_sample_fmt_is_planar((enum AVSampleFormat)f->format);
    bool is_float = f->format == AV_SAMPLE_FMT_FLTP || f->format == AV_SAMPLE_FMT_FLT;
    uint32_t fch = (uint32_t)f->ch_layout.nb_channels;
    int h = d->taps ? d->taps - 1 : 0;
    if (d->xcap < (uint32_t)h + n48) {
        free(d->x);
        d->xcap = (uint32_t)h + n48;
        d->x = malloc(d->xcap * sizeof *d->x);
    }
    for (uint32_t c = 0; c < ch; c++) {
        float *x = d->x, *hist = d->hist ? d->hist + (size_t)c * (size_t)h : NULL;
        if (h) memcpy(x, hist, (size_t)h * sizeof *x);
        for (uint32_t j = 0; j < n48; j++) {
            float v = 0;
            if (c < fch) {
                if (is_float) v = planar ? ((const float *)f->extended_data[c])[j] : ((const float *)f->data[0])[j * fch + c];
                else v = (planar ? ((const int16_t *)f->extended_data[c])[j] : ((const int16_t *)f->data[0])[j * fch + c]) / 32768.0f;
            }
            x[h + j] = v;
        }
        if (!h) {
            for (uint32_t i = 0; i < n; i++) dst[i * ch + c] = to_s16(x[i]);
            continue;
        }
        // output i is the filtered input at 48 kHz sample i * down + down - 1 (its last input); x[h + j] = input j
        for (uint32_t i = 0; i < n; i++) {
            const float *e = x + h + i * d->down + d->down - 1;
            float acc = 0;
            for (int k = 0; k < d->taps; k++) acc += d->fir[k] * e[-k];
            dst[i * ch + c] = to_s16(acc);
        }
        memcpy(hist, x + n48, (size_t)h * sizeof *x);  // (the last h inputs: x[n48 .. n48 + h))
    }
    av_frame_unref(f);
    if (d->wav && dst && n) d->wav_bytes += (uint32_t)fwrite(dst, 2 * ch, n, d->wav) * 2 * ch;
    d->decoded++;
    *consumed = len + 8;
    *samples = n;
    return 0;
}

// ------------------------------------------------------------------ IHardwareOpusDecoder
static void opus_decoder(Service *s, IpcReq *req, IpcResp *resp) {
    OpusDec *d = s->state;
    uint32_t cmd = req->cmd;
    bool perf = cmd >= 4, reset = false;
    switch (cmd) {
    case 0: case 2:  // DecodeInterleavedOld (/ForMultiStream)
    case 4: case 5:  // ...WithPerfOld
        break;
    case 6: case 7:  // ...WithPerfAndResetOld
    case 8: case 9:  // DecodeInterleaved (/ForMultiStream): reset, perf
        reset = req->data_size >= 1 && IN(uint8_t, 0) != 0;
        break;
    case 1: case 3:  // SetContext (/ForMultiStream): nothing kept between packets but FFmpeg's own state
        return;
    default:
        LOG("[opus] IHardwareOpusDecoder cmd %u (ok)\n", cmd);
        return;
    }
    if (!d) { resp->result = OPUS_RESULT_INVALID; return; }
    uint32_t consumed, samples;
    IpcBuf *ib = in_buf(req, 0), *ob = out_buf(req, 0);
    resp->result = dec_packet(d, ib, ob, reset, &consumed, &samples);
    // (LARECOMP_LOG_OPUS=1: each decoder's first 16 calls and every 500th - what the game asks and what it gets)
    if (log_opus && (d->decoded <= 16 || d->decoded % 500 == 0))
        LOG("[opus] dec %d cmd %u%s in %llu out %llu -> result %#x consumed %u samples %u\n", d->id, cmd, reset ? " reset" : "",
            ib ? (unsigned long long)ib->size : 0ull, ob ? (unsigned long long)ob->size : 0ull, resp->result, consumed, samples);
    if (resp->result) return;
    OUT(uint32_t, 0) = consumed;
    OUT(uint32_t, 4) = samples;
    if (perf) OUT(uint64_t, 8) = 0;  // (time spent decoding, in microseconds)
    resp_size(resp, perf ? 16 : 8);
}

static Service *decoder_service(OpusDec *d) {
    Service *s = service_new("IHardwareOpusDecoder", opus_decoder, d);
    s->on_close = dec_close;
    return s;
}

// ------------------------------------------------------------------ IHardwareOpusDecoderManager
// (the work buffer: memory the game sets aside for the decoder; ours lives on the host, any plausible size does)
// Without an Opus decoder in FFmpeg (a build with swresample off drops it): 0, as the logging stub answered - the game
// then opens no decoder and plays without music, where an open that fails stops it (abort on result 2111-0006).
static bool opus_available(void) {
    static int ok = -1;
    if (ok < 0) {
        ok = avcodec_find_decoder(AV_CODEC_ID_OPUS) != NULL;
        if (!ok) LOG("[opus] FFmpeg has no Opus decoder (built without swresample?): the game's music stays silent\n");
    }
    return ok;
}
static uint32_t work_size(uint32_t channels) { return opus_available() ? 0x8000 + 0x4000 * (channels ? channels : 1) : 0; }

typedef struct {  // OpusMultiStreamParameters (Ex: a use_large_frame_size flag and padding before the mapping)
    uint32_t rate, channels, streams, stereo_streams;
    const uint8_t *map;
} MultiParams;

static bool multi_params(IpcReq *req, bool ex, MultiParams *m) {
    IpcBuf *b = in_buf(req, 0);
    if (!b || b->size < 16) return false;
    const uint8_t *p = (const uint8_t *)(uintptr_t)b->addr;
    memcpy(&m->rate, p, 4), memcpy(&m->channels, p + 4, 4), memcpy(&m->streams, p + 8, 4), memcpy(&m->stereo_streams, p + 12, 4);
    uint32_t off = ex ? 24 : 16;
    m->map = b->size >= off + m->channels ? p + off : NULL;
    return true;
}

SERVICE("hwopus", hwopus) {
    (void)s;
    if (log_opus < 0) log_opus = getenv("LARECOMP_LOG_OPUS") != NULL;
    switch (req->cmd) {
    case 0: case 4: {  // OpenHardwareOpusDecoder(Ex): params (rate, channels[, large frames + padding]), work size
        uint32_t rate = IN(uint32_t, 0), ch = IN(uint32_t, 4);
        OpusDec *d = dec_open(rate, ch, 1, ch > 1 ? 1 : 0, NULL);
        if (!d) { resp->result = OPUS_RESULT_INVALID; break; }
        ipc_return_object(resp, decoder_service(d));
        break;
    }
    case 2: case 6: {  // OpenHardwareOpusDecoderForMultiStream(Ex): parameters in a buffer
        MultiParams m;
        OpusDec *d = multi_params(req, req->cmd == 6, &m)
                         ? dec_open(m.rate, m.channels, m.streams, m.stereo_streams, m.map) : NULL;
        if (!d) { resp->result = OPUS_RESULT_INVALID; break; }
        ipc_return_object(resp, decoder_service(d));
        break;
    }
    case 1: case 5: case 8:  // GetWorkBufferSize(Ex / ExEx)
        OUT(uint32_t, 0) = work_size(IN(uint32_t, 4));
        resp_size(resp, 4);
        break;
    case 3: case 7: case 9: {  // GetWorkBufferSizeForMultiStream(Ex / ExEx)
        MultiParams m = {0};
        multi_params(req, req->cmd != 3, &m);
        OUT(uint32_t, 0) = work_size(m.channels);
        resp_size(resp, 4);
        break;
    }
    default:
        LOG("[opus] IHardwareOpusDecoderManager cmd %u (ok)\n", req->cmd);
        resp_size(resp, 0x10);
    }
}
