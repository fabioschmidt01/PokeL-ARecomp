using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
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

namespace PokeLExtractor;

public static class PackageScanner
{
    private static readonly Dictionary<ulong, string> KnownTitles = new()
    {
        { 0x01001F5010DFA000, "Pokémon Legends: Arceus" },
        { 0x0100F43008C44000, "Pokémon Legends: Z-A" },
        { 0x0100ABF008968000, "Pokémon Sword" },
        { 0x01008DB008C2C000, "Pokémon Shield" },
        { 0x0100A3D008C5C000, "Pokémon Scarlet" },
        { 0x01008C5008C5A000, "Pokémon Violet" },
        { 0x0100000000010000, "Super Mario Odyssey" },
        { 0x01007EF00011E000, "The Legend of Zelda: Breath of the Wild" },
        { 0x0100F2C0115B6000, "The Legend of Zelda: Tears of the Kingdom" },
        { 0x01006BB00C6F0000, "The Legend of Zelda: Link's Awakening" }
    };

    public static List<string> FindPackageFiles(string rootDir, bool recursive)
    {
        var results = new List<string>();
        if (!Directory.Exists(rootDir)) return results;

        var opt = recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
        var validExts = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".nsp", ".xci" };

        try
        {
            var files = Directory.EnumerateFiles(rootDir, "*.*", opt);
            foreach (var f in files)
            {
                if (validExts.Contains(Path.GetExtension(f)))
                    results.Add(f);
            }
        }
        catch (UnauthorizedAccessException) { }
        catch (Exception) { }

        return results;
    }

    public static (string? prodKeys, string? titleKeys) FindKeysFiles(string rootDir, bool recursive)
    {
        string? prod = null;
        string? title = null;
        if (!Directory.Exists(rootDir)) return (null, null);

        var opt = recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
        var keyExts = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".keys", ".key", ".txt" };

        try
        {
            var candidates = Directory.EnumerateFiles(rootDir, "*.*", opt)
                .Where(f => keyExts.Contains(Path.GetExtension(f)) && new FileInfo(f).Length < 2 * 1024 * 1024)
                .OrderBy(f => Path.GetFileName(f).Equals("prod.keys", StringComparison.OrdinalIgnoreCase) ? 0 : 1);

            foreach (var file in candidates)
            {
                try
                {
                    var lines = File.ReadLines(file).Take(50);
                    foreach (var line in lines)
                    {
                        var trimmed = line.Trim();
                        if (trimmed.StartsWith("header_key", StringComparison.OrdinalIgnoreCase))
                        {
                            prod ??= file;
                            break;
                        }
                        if (trimmed.Length == 67 && trimmed[32] == '=' && trimmed.Replace(" ", "").Length == 65)
                        {
                            title ??= file;
                        }
                    }
                }
                catch { }

                if (prod != null && title != null) break;
            }
        }
        catch { }

        return (prod, title);
    }

    public static KeySet LoadKeySet(string prodKeysPath, string? titleKeysPath = null)
    {
        string? title = titleKeysPath;
        if (string.IsNullOrEmpty(title))
        {
            string dir = Path.GetDirectoryName(Path.GetFullPath(prodKeysPath))!;
            title = new[] { "title.keys", "title.keys.txt" }
                .Select(n => Path.Combine(dir, n))
                .FirstOrDefault(File.Exists);
        }

        return ExternalKeyReader.ReadKeyFile(prodKeysPath, title);
    }

    public static SwitchPackageInfo? InspectPackage(string pkgPath, KeySet keySet)
    {
        try
        {
            var fi = new FileInfo(pkgPath);
            var info = new SwitchPackageInfo
            {
                FilePath = pkgPath,
                FileName = fi.Name,
                FileSize = fi.Length
            };

            using var st = new LocalStorage(pkgPath, FileAccess.Read);
            IFileSystem fs;
            if (IsXci(st))
            {
                var xci = new Xci(keySet, st);
                if (!xci.HasPartition(XciPartitionType.Secure)) return null;
                fs = xci.OpenPartition(XciPartitionType.Secure);
            }
            else
            {
                var pfs = new PartitionFileSystem();
                if (pfs.Initialize(st).IsFailure()) return null;
                fs = pfs;
            }

            // Register tickets if present
            foreach (var e in fs.EnumerateEntries("/", "*.tik"))
            {
                using var f = new UniqueRef<IFile>();
                if (fs.OpenFile(ref f.Ref, e.FullPath.ToU8Span(), OpenMode.Read).IsSuccess())
                {
                    var t = new Ticket(f.Get.AsStream());
                    var key = t.GetTitleKey(keySet);
                    if (key != null) keySet.ExternalKeySet.Add(new RightsId(t.RightsId), new AccessKey(key));
                }
            }

            Nca? controlNca = null;
            Nca? programNca = null;
            Nca? publicDataNca = null;

            foreach (var e in fs.EnumerateEntries("/", "*.nca"))
            {
                var f = new UniqueRef<IFile>();
                if (fs.OpenFile(ref f.Ref, e.FullPath.ToU8Span(), OpenMode.Read).IsSuccess())
                {
                    try
                    {
                        var nca = new Nca(keySet, f.Release().AsStorage());
                        ulong tid = nca.Header.TitleId;
                        if (info.TitleId == 0) info.TitleId = tid;

                        if (nca.Header.ContentType == NcaContentType.Program)
                            programNca = nca;
                        else if (nca.Header.ContentType == NcaContentType.Control)
                            controlNca = nca;
                        else if (nca.Header.ContentType == NcaContentType.PublicData)
                            publicDataNca = nca;
                    }
                    catch { }
                }
            }

            if (programNca != null)
            {
                info.TitleId = programNca.Header.TitleId;
                info.BaseTitleId = info.TitleId & ~0x1FFFUL;

                if (IsPatch(programNca) || (info.TitleId & 0x1FFFUL) == 0x800)
                {
                    info.Type = PackageType.Update;
                }
                else
                {
                    info.Type = PackageType.BaseGame;
                }
            }
            else if (publicDataNca != null)
            {
                info.TitleId = publicDataNca.Header.TitleId;
                info.BaseTitleId = info.TitleId & ~0x1FFFUL;
                info.Type = PackageType.Dlc;
            }
            else if (info.TitleId != 0)
            {
                info.BaseTitleId = info.TitleId & ~0x1FFFUL;
                ulong low = info.TitleId & 0x1FFFUL;
                info.Type = low == 0 ? PackageType.BaseGame : low == 0x800 ? PackageType.Update : PackageType.Dlc;
            }

            // Extract Name, Version and Icon from Control NCA if available
            if (controlNca != null)
            {
                try
                {
                    using var controlFs = controlNca.OpenFileSystem(NcaSectionType.Data, IntegrityCheckLevel.None);
                    ParseControlMetadata(controlFs, info);
                }
                catch { }
            }

            if (string.IsNullOrEmpty(info.GameTitle) && KnownTitles.TryGetValue(info.BaseTitleId, out var knownName))
            {
                info.GameTitle = knownName;
            }

            if (string.IsNullOrEmpty(info.GameTitle))
            {
                // Clean filename fallback
                string clean = Path.GetFileNameWithoutExtension(info.FileName);
                int bracket = clean.IndexOf('[');
                if (bracket > 0) clean = clean.Substring(0, bracket).Trim();
                info.GameTitle = clean;
            }

            return info;
        }
        catch
        {
            return null;
        }
    }

    private static void ParseControlMetadata(IFileSystem controlFs, SwitchPackageInfo info)
    {
        // 1. Read control.nacp
        using var nacpFile = new UniqueRef<IFile>();
        if (controlFs.OpenFile(ref nacpFile.Ref, "/control.nacp".ToU8Span(), OpenMode.Read).IsSuccess())
        {
            nacpFile.Get.GetSize(out long nacpSize).ThrowIfFailure();
            var buf = new byte[Math.Min(nacpSize, 0x4000)];
            nacpFile.Get.Read(out _, 0, buf).ThrowIfFailure();

            // Language priorities: Portuguese (index 10 / 15), AmericanEnglish (0), BritishEnglish (1), Spanish (6), Japanese (2)
            int[] langPriorities = { 10, 15, 0, 1, 6, 2, 3, 4, 5 };
            foreach (int langIdx in langPriorities)
            {
                int offset = langIdx * 0x300;
                if (offset + 0x200 <= buf.Length)
                {
                    string name = ReadNullTerminatedUtf8(buf, offset, 0x200);
                    if (!string.IsNullOrWhiteSpace(name))
                    {
                        info.GameTitle = name;
                        break;
                    }
                }
            }

            // DisplayVersion at 0x3060 (16 bytes)
            if (buf.Length >= 0x3070)
            {
                string ver = ReadNullTerminatedUtf8(buf, 0x3060, 16);
                if (!string.IsNullOrWhiteSpace(ver)) info.DisplayVersion = ver;
            }
        }

        // 2. Read Icon (JPEG format, 256x256)
        string[] iconCandidates = {
            "/icon_AmericanEnglish.dat",
            "/icon_Portuguese.dat",
            "/icon_BrazilianPortuguese.dat",
            "/icon_BritishEnglish.dat",
            "/icon_Spanish.dat",
            "/icon_Japanese.dat"
        };

        foreach (var iconPath in iconCandidates)
        {
            using var iconFile = new UniqueRef<IFile>();
            if (controlFs.OpenFile(ref iconFile.Ref, iconPath.ToU8Span(), OpenMode.Read).IsSuccess())
            {
                iconFile.Get.GetSize(out long iconSize).ThrowIfFailure();
                var iconBytes = new byte[iconSize];
                iconFile.Get.Read(out _, 0, iconBytes).ThrowIfFailure();
                using var ms = new MemoryStream(iconBytes);
                info.Icon = Image.FromStream(ms);
                break;
            }
        }
    }

    private static string ReadNullTerminatedUtf8(byte[] buffer, int offset, int length)
    {
        int end = offset;
        while (end < offset + length && end < buffer.Length && buffer[end] != 0) end++;
        return Encoding.UTF8.GetString(buffer, offset, end - offset).Trim();
    }

    public static bool IsPatch(Nca nca)
    {
        int i = Nca.GetSectionIndexFromType(NcaSectionType.Data, nca.Header.ContentType);
        return nca.Header.IsSectionEnabled(i) && nca.Header.GetFsHeader(i).IsPatchSection();
    }

    public static bool IsXci(IStorage st)
    {
        var magic = new byte[4];
        return st.Read(0x100, magic).IsSuccess() && magic.AsSpan().SequenceEqual("HEAD"u8);
    }
}
