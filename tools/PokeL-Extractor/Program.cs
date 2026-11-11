using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows.Forms;

namespace PokeLExtractor;

public static class Program
{
    [System.Runtime.InteropServices.DllImport("kernel32.dll")]
    private static extern bool AttachConsole(int dwProcessId);
    private const int ATTACH_PARENT_PROCESS = -1;

    [STAThread]
    public static void Main(string[] args)
    {
        if (args.Length > 0 && (args.Contains("--input") || args.Contains("-i") || args.Contains("--help") || args.Contains("-h")))
        {
            if (AttachConsole(ATTACH_PARENT_PROCESS))
            {
                var stdOut = new StreamWriter(Console.OpenStandardOutput()) { AutoFlush = true };
                Console.SetOut(stdOut);
                var stdErr = new StreamWriter(Console.OpenStandardError()) { AutoFlush = true };
                Console.SetError(stdErr);
            }
            RunCli(args);
            return;
        }

        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }

    private static void RunCli(string[] args)
    {
        Console.WriteLine("==================================================================");
        Console.WriteLine("PokeL-Extractor CLI — Extrator e Mesclador Standalone LibHac");
        Console.WriteLine("==================================================================");

        string? inputDir = null;
        string? outDir = null;
        string? keysPath = null;
        string? titleKeysPath = null;
        bool recursive = true;

        for (int i = 0; i < args.Length; i++)
        {
            if ((args[i] == "--input" || args[i] == "-i") && i + 1 < args.Length) inputDir = args[++i];
            else if ((args[i] == "--output" || args[i] == "-o") && i + 1 < args.Length) outDir = args[++i];
            else if ((args[i] == "--keys" || args[i] == "-k") && i + 1 < args.Length) keysPath = args[++i];
            else if (args[i] == "--title-keys" && i + 1 < args.Length) titleKeysPath = args[++i];
            else if (args[i] == "--no-recursive") recursive = false;
        }

        if (string.IsNullOrEmpty(inputDir) || string.IsNullOrEmpty(outDir))
        {
            Console.WriteLine("Uso: PokeL-Extractor.exe --input <pasta_dumps> --output <pasta_extraida> [--keys <prod.keys>] [--no-recursive]");
            return;
        }

        if (string.IsNullOrEmpty(keysPath))
        {
            var (prod, title) = PackageScanner.FindKeysFiles(inputDir, recursive);
            keysPath = prod;
            titleKeysPath ??= title;
        }

        if (string.IsNullOrEmpty(keysPath) || !File.Exists(keysPath))
        {
            Console.WriteLine("Erro: prod.keys não encontrado. Especifique com --keys <caminho>.");
            return;
        }

        Console.WriteLine($"[1/4] Carregando chaves de: {keysPath}");
        var keySet = PackageScanner.LoadKeySet(keysPath, titleKeysPath);
        if (keySet.HeaderKey.IsZeros())
        {
            Console.WriteLine("Erro: O prod.keys não possui header_key válida.");
            return;
        }

        Console.WriteLine($"[2/4] Escaneando arquivos em {inputDir} (Recursivo: {recursive})...");
        var files = PackageScanner.FindPackageFiles(inputDir, recursive);
        Console.WriteLine($"      Encontrados {files.Count} pacotes.");

        var packages = new System.Collections.Generic.List<SwitchPackageInfo>();
        foreach (var f in files)
        {
            var info = PackageScanner.InspectPackage(f, keySet);
            if (info != null)
            {
                packages.Add(info);
                Console.WriteLine($"      -> [{info.TypeDescription}] {info.GameTitle} ({info.DisplaySize}) - {info.FileName}");
            }
        }

        if (packages.Count == 0)
        {
            Console.WriteLine("Erro: Nenhum jogo ou atualização do Switch foi identificado.");
            return;
        }

        Console.WriteLine($"[3/4] Extraindo e mesclando RomFS BKTR para: {outDir}...");
        var engine = new ExtractorEngine();
        engine.OnLog += (level, msg) => Console.WriteLine($"[{level}] {msg}");
        engine.OnProgress += (prog) =>
        {
            Console.Write($"\r{prog.Operation} {prog.Percentage:F1}% [{SwitchPackageInfo.FormatBytes(prog.BytesDone)} / {SwitchPackageInfo.FormatBytes(prog.TotalBytes)}] {prog.SpeedMBs:F1} MB/s   ");
        };

        var ok = engine.ExtractAsync(keySet, outDir, packages, CancellationToken.None).GetAwaiter().GetResult();
        Console.WriteLine();
        if (ok)
        {
            Console.WriteLine("[4/4] Concluído com sucesso! Pasta de destino pronta para o PokeL-ARecomp.");
        }
        else
        {
            Console.WriteLine("Erro durante a extração.");
        }
    }
}
