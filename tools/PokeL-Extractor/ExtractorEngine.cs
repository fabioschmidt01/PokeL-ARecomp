using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using LibHac;
using LibHac.Common;
using LibHac.Common.Keys;
using LibHac.Fs;
using LibHac.Fs.Fsa;
using LibHac.FsSystem;
using LibHac.Spl;
using LibHac.Tools.Es;
using LibHac.Tools.Fs;
using LibHac.Tools.FsSystem;
using LibHac.Tools.FsSystem.NcaUtils;
using Path = System.IO.Path;
using TimeSpan = System.TimeSpan;

namespace PokeLExtractor;

public enum LogType
{
    Info,
    Success,
    Warning,
    Error
}

public class ProgressInfo
{
    public long BytesDone { get; set; }
    public long TotalBytes { get; set; }
    public double Percentage => TotalBytes > 0 ? (double)BytesDone / TotalBytes * 100.0 : 0.0;
    public double SpeedMBs { get; set; }
    public string Operation { get; set; } = "";
    public TimeSpan Eta { get; set; }
}

public class ExtractorEngine
{
    public event Action<LogType, string>? OnLog;
    public event Action<ProgressInfo>? OnProgress;

    public async Task<bool> ExtractAsync(
        KeySet keySet, 
        string outDir, 
        List<SwitchPackageInfo> packages, 
        CancellationToken ct)
    {
        return await Task.Run(() =>
        {
            try
            {
                OnLog?.Invoke(LogType.Info, "Iniciando processo de extração e mesclagem...");

                if (keySet.HeaderKey.IsZeros())
                {
                    OnLog?.Invoke(LogType.Error, "O prod.keys não possui header_key válida. Certifique-se de que é um dump completo.");
                    return false;
                }

                var selectedPkgs = packages.Where(p => p.IsSelected).ToList();
                if (selectedPkgs.Count == 0)
                {
                    OnLog?.Invoke(LogType.Warning, "Nenhum pacote selecionado para extração.");
                    return false;
                }

                Directory.CreateDirectory(outDir);

                // 1. Coleta todas as NCAs de todos os pacotes selecionados
                var ncas = new List<(Nca nca, string name, string pkgPath)>();
                var openStorages = new List<LocalStorage>();

                try
                {
                    foreach (var pkg in selectedPkgs)
                    {
                        ct.ThrowIfCancellationRequested();
                        OnLog?.Invoke(LogType.Info, $"Lendo pacote: {pkg.FileName} ({pkg.TypeDescription})...");

                        var st = new LocalStorage(pkg.FilePath, FileAccess.Read);
                        openStorages.Add(st);
                        IFileSystem fs;

                        if (PackageScanner.IsXci(st))
                        {
                            var xci = new Xci(keySet, st);
                            if (!xci.HasPartition(XciPartitionType.Secure))
                            {
                                OnLog?.Invoke(LogType.Warning, $"{pkg.FileName}: sem partição secure, ignorando.");
                                continue;
                            }
                            fs = xci.OpenPartition(XciPartitionType.Secure);
                        }
                        else
                        {
                            var pfs = new PartitionFileSystem();
                            if (pfs.Initialize(st).IsFailure())
                            {
                                OnLog?.Invoke(LogType.Warning, $"{pkg.FileName}: falha ao ler contêiner NSP.");
                                continue;
                            }
                            fs = pfs;
                        }

                        // Registra tickets
                        foreach (var e in fs.EnumerateEntries("/", "*.tik"))
                        {
                            using var f = new UniqueRef<IFile>();
                            if (fs.OpenFile(ref f.Ref, e.FullPath.ToU8Span(), OpenMode.Read).IsSuccess())
                            {
                                var t = new Ticket(f.Get.AsStream());
                                var key = t.GetTitleKey(keySet);
                                if (key != null)
                                {
                                    keySet.ExternalKeySet.Add(new RightsId(t.RightsId), new AccessKey(key));
                                    OnLog?.Invoke(LogType.Success, $"Chave de título registrada via ticket ({Convert.ToHexString(t.RightsId)}).");
                                }
                            }
                        }

                        foreach (var e in fs.EnumerateEntries("/", "*.nca"))
                        {
                            var f = new UniqueRef<IFile>();
                            if (fs.OpenFile(ref f.Ref, e.FullPath.ToU8Span(), OpenMode.Read).IsSuccess())
                            {
                                try
                                {
                                    var nca = new Nca(keySet, f.Release().AsStorage());
                                    ncas.Add((nca, $"{pkg.FileName}/{e.Name}", pkg.FilePath));
                                }
                                catch (Exception ex)
                                {
                                    OnLog?.Invoke(LogType.Warning, $"NCA {e.Name} não pôde ser aberta: {ex.Message}");
                                }
                            }
                        }
                    }

                    // 2. Classifica Base, Update e DLCs
                    ulong targetBaseId = selectedPkgs.First().BaseTitleId;
                    Nca? baseNca = null;
                    Nca? updateNca = null;
                    var dlcNcas = new List<Nca>();

                    foreach (var (nca, name, _) in ncas)
                    {
                        ulong tid = nca.Header.TitleId;
                        if ((tid & ~0x1FFFUL) != targetBaseId) continue;

                        if (nca.Header.ContentType == NcaContentType.Program)
                        {
                            if (PackageScanner.IsPatch(nca) || (tid & 0x1FFFUL) == 0x800)
                            {
                                updateNca = nca;
                                OnLog?.Invoke(LogType.Success, $"NCA de Atualização encontrada: Title {tid:x16}");
                            }
                            else if (tid == targetBaseId)
                            {
                                baseNca = nca;
                                OnLog?.Invoke(LogType.Success, $"NCA de Jogo Base encontrada: Title {tid:x16}");
                            }
                        }
                        else if (nca.Header.ContentType == NcaContentType.PublicData)
                        {
                            dlcNcas.Add(nca);
                            OnLog?.Invoke(LogType.Success, $"NCA de DLC encontrada: Title {tid:x16}");
                        }
                    }

                    if (baseNca == null)
                    {
                        OnLog?.Invoke(LogType.Error, "O Jogo Base não foi encontrado entre os pacotes selecionados.");
                        return false;
                    }

                    // 3. Validação de chaves de título
                    foreach (var n in new[] { baseNca, updateNca }.Concat(dlcNcas))
                    {
                        if (n != null && n.Header.HasRightsId && !keySet.ExternalKeySet.Contains(new RightsId(n.Header.RightsId)))
                        {
                            OnLog?.Invoke(LogType.Error, $"Falta a chave de título para {n.Header.TitleId:x16}. Coloque o title.keys na pasta do jogo.");
                            return false;
                        }
                    }

                    // 4. Extração do ExeFS
                    ct.ThrowIfCancellationRequested();
                    string exefsDir = Path.Combine(outDir, "exefs");
                    Directory.CreateDirectory(exefsDir);
                    var codeNca = updateNca ?? baseNca;
                    string codeSource = updateNca != null ? "Atualização (Update)" : "Jogo Base";

                    OnLog?.Invoke(LogType.Info, $"Extraindo código executável (exefs/) a partir de {codeSource}...");

                    using (var exefs = codeNca.OpenFileSystem(NcaSectionType.Code, IntegrityCheckLevel.None))
                    {
                        foreach (var e in exefs.EnumerateEntries("/", "*"))
                        {
                            ct.ThrowIfCancellationRequested();
                            using var f = new UniqueRef<IFile>();
                            exefs.OpenFile(ref f.Ref, e.FullPath.ToU8Span(), OpenMode.Read).ThrowIfFailure();
                            f.Get.GetSize(out long sz).ThrowIfFailure();
                            var data = new byte[sz];
                            f.Get.Read(out _, 0, data).ThrowIfFailure();
                            File.WriteAllBytes(Path.Combine(exefsDir, e.Name), data);
                            OnLog?.Invoke(LogType.Info, $"  -> exefs/{e.Name} ({SwitchPackageInfo.FormatBytes(sz)})");
                        }
                    }
                    OnLog?.Invoke(LogType.Success, "Extração do ExeFS concluída com sucesso!");

                    // 5. Preparação das imagens RomFS e DLCs
                    var images = new List<(IStorage st, string path, string desc)>();
                    if (updateNca != null)
                    {
                        OnLog?.Invoke(LogType.Info, "Preparando fusão BKTR: RomFS Base + Patch de Atualização...");
                        images.Add((baseNca.OpenStorageWithPatch(updateNca, NcaSectionType.Data, IntegrityCheckLevel.None),
                                    Path.Combine(outDir, "romfs.bin"),
                                    "RomFS com Update 1.1.1"));
                    }
                    else
                    {
                        OnLog?.Invoke(LogType.Info, "Preparando RomFS Base (sem atualização)...");
                        images.Add((baseNca.OpenStorage(NcaSectionType.Data, IntegrityCheckLevel.None),
                                    Path.Combine(outDir, "romfs.bin"),
                                    "RomFS Base"));
                    }

                    if (dlcNcas.Count > 0)
                    {
                        string aocDir = Path.Combine(outDir, "aoc");
                        Directory.CreateDirectory(aocDir);
                        foreach (var d in dlcNcas)
                        {
                            images.Add((d.OpenStorage(NcaSectionType.Data, IntegrityCheckLevel.None),
                                        Path.Combine(aocDir, $"{d.Header.TitleId:x16}.bin"),
                                        $"DLC {d.Header.TitleId:x16}"));
                        }
                    }

                    // 6. Cálculo do tamanho total
                    long totalBytes = 0;
                    foreach (var (st, _, _) in images)
                    {
                        st.GetSize(out long s).ThrowIfFailure();
                        totalBytes += s;
                    }

                    OnLog?.Invoke(LogType.Info, $"Total de dados a serem gerados: {SwitchPackageInfo.FormatBytes(totalBytes)}");

                    // 7. Streaming com barra de progresso, velocidade e ETA
                    long doneBytes = 0;
                    var buf = new byte[8 << 20]; // 8 MB buffer
                    var sw = Stopwatch.StartNew();
                    long lastDone = 0;
                    var lastTime = sw.Elapsed;

                    foreach (var (st, path, desc) in images)
                    {
                        ct.ThrowIfCancellationRequested();
                        OnLog?.Invoke(LogType.Info, $"Gravando {desc} -> {Path.GetFileName(path)}...");
                        st.GetSize(out long size).ThrowIfFailure();

                        using var f = File.Create(path + ".part");
                        for (long off = 0; off < size; off += buf.Length)
                        {
                            ct.ThrowIfCancellationRequested();
                            int n = (int)Math.Min(buf.Length, size - off);
                            st.Read(off, buf.AsSpan(0, n)).ThrowIfFailure();
                            f.Write(buf, 0, n);
                            doneBytes += n;

                            var now = sw.Elapsed;
                            var dt = (now - lastTime).TotalSeconds;
                            double speedMBs = 0;
                            if (dt >= 0.5)
                            {
                                speedMBs = (doneBytes - lastDone) / (1024.0 * 1024.0) / dt;
                                lastDone = doneBytes;
                                lastTime = now;
                            }

                            TimeSpan eta = speedMBs > 0 
                                ? TimeSpan.FromSeconds((totalBytes - doneBytes) / (speedMBs * 1024 * 1024))
                                : TimeSpan.Zero;

                            OnProgress?.Invoke(new ProgressInfo
                            {
                                BytesDone = doneBytes,
                                TotalBytes = totalBytes,
                                SpeedMBs = speedMBs,
                                Operation = desc,
                                Eta = eta
                            });
                        }
                        f.Close();
                        File.Move(path + ".part", path, true);
                        OnLog?.Invoke(LogType.Success, $"Gravação concluída: {Path.GetFileName(path)} ({SwitchPackageInfo.FormatBytes(size)})");
                    }

                    sw.Stop();
                    OnLog?.Invoke(LogType.Success, $"🎉 Processo 100% CONCLUÍDO em {sw.Elapsed.TotalSeconds:F1}s!");
                    OnLog?.Invoke(LogType.Success, $"Arquivos prontos para o Recomp na pasta: {outDir}");

                    OnProgress?.Invoke(new ProgressInfo
                    {
                        BytesDone = totalBytes,
                        TotalBytes = totalBytes,
                        SpeedMBs = 0,
                        Operation = "Concluído!",
                        Eta = TimeSpan.Zero
                    });

                    return true;
                }
                finally
                {
                    foreach (var s in openStorages) s.Dispose();
                }
            }
            catch (OperationCanceledException)
            {
                OnLog?.Invoke(LogType.Warning, "Operação cancelada pelo usuário.");
                return false;
            }
            catch (Exception ex)
            {
                OnLog?.Invoke(LogType.Error, $"Erro durante a extração: {ex.Message}");
                return false;
            }
        });
    }
}
