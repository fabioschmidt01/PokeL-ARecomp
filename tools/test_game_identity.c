// Game identity (runtime/game_identity.h): only Pokémon Legends: Arceus 1.1.1's exefs/main is accepted; Violet, Z-A
// and Link's Awakening are recognised by name and refused, unknown builds refused. Builds fake NSO headers in a scratch
// folder: tools/test_game_identity.py <empty scratch dir>.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_identity.h"

static int failures;
#define CHECK(c)                                                                                \
    do {                                                                                        \
        if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } \
    } while (0)

static const uint8_t ARCEUS_111[8] = {0xae, 0xe8, 0xf1, 0x50, 0xdd, 0xa1, 0xb5, 0xa8};
static const uint8_t VIOLET_301[8] = {0x70, 0x9b, 0xfd, 0x66, 0x11, 0x52, 0x98, 0x64};
static const uint8_t ZA_100[8] = {0x72, 0x22, 0xe1, 0x3e, 0xcf, 0x6a, 0xdb, 0x32};
static const uint8_t ZA_200[8] = {0x8a, 0x1c, 0x86, 0xc4, 0x37, 0x39, 0x4b, 0x69};
static const uint8_t ZA_202[8] = {0xb1, 0xf1, 0x2f, 0xd9, 0x19, 0xea, 0xe8, 0x6a};
static const uint8_t LA_100[8] = {0xae, 0x16, 0xf7, 0x1e, 0x00, 0x2a, 0xf8, 0xcb};

// A main module: "NSO0", padding, the build ID at 0x40 (len < 0x48: cut short)
static void put_main(const char *path, const uint8_t id[8], size_t len) {
    uint8_t buf[0x80] = {'N', 'S', 'O', '0'};
    memcpy(buf + 0x40, id, 8);
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "can't write %s\n", path); exit(2); }
    fwrite(buf, 1, len, f);
    fclose(f);
}

static void refused(const char *dir, const char *name, const uint8_t id[8], const char *want_desc) {
    char p[1100], desc[160];
    uint8_t got[8];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    put_main(p, id, 0x80);
    CHECK(game_identity_read(p, got) && !memcmp(got, id, 8));
    CHECK(game_identity_supported(got) == NULL);
    game_identity_describe(got, desc, sizeof desc);
    if (strcmp(desc, want_desc)) {
        fprintf(stderr, "FAIL %s: described as \"%s\", want \"%s\"\n", name, desc, want_desc);
        failures++;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]); return 2; }
    const char *dir = argv[1];
    char p[1100], desc[160];
    uint8_t id[8];

    // Pokémon Legends: Arceus 1.1.1: the one build accepted
    snprintf(p, sizeof p, "%s/arceus_main", dir);
    put_main(p, ARCEUS_111, 0x80);
    CHECK(game_identity_read(p, id));
    CHECK(game_identity_supported(id) && !strcmp(game_identity_supported(id), "1.1.1"));
    game_identity_describe(id, desc, sizeof desc);
    CHECK(!strcmp(desc, "Pok\xc3\xa9mon Legends: Arceus 1.1.1"));

    // the related recomps' builds: recognised, refused by name (the start screen says whose files they are)
    refused(dir, "violet_main", VIOLET_301, "Pok\xc3\xa9mon Violet 3.0.1");
    refused(dir, "za100_main", ZA_100, "Pok\xc3\xa9mon Legends: Z-A 1.0.0");
    refused(dir, "za200_main", ZA_200, "Pok\xc3\xa9mon Legends: Z-A 2.0.0");
    refused(dir, "za202_main", ZA_202, "Pok\xc3\xa9mon Legends: Z-A 2.0.2");
    refused(dir, "la_main", LA_100, "The Legend of Zelda: Link's Awakening 1.0.0");

    // anything else (another Arceus version, a corrupt file): refused, its ID shown
    static const uint8_t other[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    refused(dir, "other_main", other, "an unknown build (0123456789abcdef)");
    uint8_t near[8];  // (one bit off Arceus's: matched byte for byte, not by prefix)
    memcpy(near, ARCEUS_111, 8), near[7] ^= 1;
    CHECK(game_identity_supported(near) == NULL);
    CHECK(game_identity_find(near) == NULL);

    // no file / too short to hold a build ID: not read (so never accepted)
    snprintf(p, sizeof p, "%s/missing_main", dir);
    CHECK(!game_identity_read(p, id));
    snprintf(p, sizeof p, "%s/short_main", dir);
    put_main(p, ARCEUS_111, 0x44);
    CHECK(!game_identity_read(p, id));

    // exactly one supported entry, and it is Arceus 1.1.1
    int supported = 0;
    for (size_t i = 0; i < sizeof GAME_IDENTITIES / sizeof *GAME_IDENTITIES; i++)
        if (GAME_IDENTITIES[i].supported) {
            supported++;
            CHECK(!memcmp(GAME_IDENTITIES[i].id, ARCEUS_111, 8));
        }
    CHECK(supported == 1);

    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("all checks passed\n");
    return 0;
}
