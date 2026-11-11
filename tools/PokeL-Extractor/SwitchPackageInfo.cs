using System;
using System.Drawing;

namespace PokeLExtractor;

public enum PackageType
{
    Unknown,
    BaseGame,
    Update,
    Dlc
}

public class SwitchPackageInfo
{
    public string FilePath { get; set; } = "";
    public string FileName { get; set; } = "";
    public long FileSize { get; set; }
    public string DisplaySize => FormatBytes(FileSize);
    public ulong TitleId { get; set; }
    public string TitleIdHex => $"{TitleId:x16}";
    public ulong BaseTitleId { get; set; }
    public string BaseTitleIdHex => $"{BaseTitleId:x16}";
    public PackageType Type { get; set; }
    public string GameTitle { get; set; } = "";
    public string DisplayVersion { get; set; } = "";
    public uint VersionNumber { get; set; }
    public Image? Icon { get; set; }
    public bool IsSelected { get; set; } = true;

    public string TypeDescription => Type switch
    {
        PackageType.BaseGame => "🎮 Jogo Base",
        PackageType.Update => $"🔄 Atualização ({(!string.IsNullOrEmpty(DisplayVersion) ? DisplayVersion : $"v{VersionNumber}")})",
        PackageType.Dlc => "🎁 DLC / Conteúdo Extra",
        _ => "❓ Desconhecido"
    };

    public static string FormatBytes(long bytes)
    {
        string[] suffixes = { "B", "KB", "MB", "GB", "TB" };
        int counter = 0;
        decimal number = bytes;
        while (Math.Round(number / 1024) >= 1 && counter < suffixes.Length - 1)
        {
            number /= 1024;
            counter++;
        }
        return $"{number:n2} {suffixes[counter]}";
    }
}
