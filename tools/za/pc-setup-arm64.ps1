# PokeL-ARecomp for Windows ARM64 / Android (GameHub): the game files and the ARM64 game code, made on a Windows PC.
# The start screen's setup does the same on the device (runtime/gamebuild.c), but on an Android tablet its generator
# runs emulated (lasgen is an x64 program, and it emulates ARM64 code itself to find jump tables): hours instead of
# minutes. Here everything runs natively and the result - the extracted folder - is copied to the device.
#
# Usage (PC-Setup.bat runs this): put your dump (.nsp / .xci, base game + 1.1.1 update or a dump that already holds
# it) and prod.keys in gamefiles\ next to this file, run it, then copy the whole PokeL-ARecomp folder to the device.
# Made from tools/za/pc-setup-arm64.ps1 by tools/za/dist-arm64.sh, which fills in the ids below.
param([string]$GameFiles = "", [int]$Jobs = 0)
$ErrorActionPreference = 'Stop'
$R = $PSScriptRoot
if (-not $GameFiles) { $GameFiles = Join-Path $R 'gamefiles' }
if ($Jobs -le 0) { $Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 1) }
$GamelibId = '@GAMELIB_ID@'                      # (the ARM64 exe's GAMELIB_ID: Makefile)
$CFlags = '@GAMELIB_CFLAGS@' -split ' '          # (GAMELIB_CFLAGS: the game code's flags)
$Shift = '@GUEST_SHIFT_DEC@'                     # (GUEST_SHIFT, runtime/rt.h)
$Bases = @{ main = '0x1000000000'; sdk = '0x1008000000'; subsdk0 = '0x100a000000'; subsdk1 = '0x100b000000' }
$Out = Join-Path $R 'extracted'
$Work = Join-Path $Out '.pcbuild'
$Zig = Join-Path $R 'zig\zig-x64.exe'
$Lib = Join-Path $R 'zig\lib'
$Src = Join-Path $R 'gamelib-src'
$T0 = Get-Date

function Say($s) { Write-Host ("[{0:mm\:ss}] {1}" -f ((Get-Date) - $T0), $s) }
function Quote($a) { if ($a -match '[\s"]') { '"' + ($a -replace '"', '\"') + '"' } else { $a } }
function Start-Tool($exe, [string[]]$argv) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = ($argv | ForEach-Object { Quote $_ }) -join ' '
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardOutput = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    # (read both pipes in the background: a full pipe would stall the tool)
    $o = $p.StandardOutput.ReadToEndAsync(); $e = $p.StandardError.ReadToEndAsync()
    [pscustomobject]@{ P = $p; Out = $o; Err = $e }
}
function Tool-Text($t) { ($t.Out.Result + $t.Err.Result).Trim() }

foreach ($f in @($Zig, (Join-Path $R 'lasgen\lasgen.exe'), (Join-Path $R 'zaimport\zaimport.exe'), (Join-Path $R 'PokeL-ARecomp.dll.a'))) {
    if (-not (Test-Path $f)) { throw "missing $f - unzip the whole PokeL-ARecomp-windows-arm64.zip and run this inside it" }
}

# 1. the game files: exefs\ and romfs.bin from the dump (zaimport, as the start screen)
if ((Test-Path (Join-Path $Out 'romfs.bin')) -and (Test-Path (Join-Path $Out 'exefs\main'))) {
    Say "game files already imported ($Out)"
} else {
    $keys = Get-ChildItem $GameFiles -Recurse -File -Filter 'prod.keys' -ErrorAction SilentlyContinue | Select-Object -First 1
    $pkgs = @(Get-ChildItem $GameFiles -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.nsp', '.xci' })
    if (-not $keys) { throw "no prod.keys in $GameFiles" }
    if ($pkgs.Count -eq 0) { throw "no .nsp / .xci in $GameFiles" }
    Say "importing $($pkgs.Count) package(s) with $($keys.FullName)"
    $argv = @($keys.FullName, $Out) + ($pkgs | ForEach-Object { $_.FullName })
    $t = Start-Tool (Join-Path $R 'zaimport\zaimport.exe') $argv
    $t.P.WaitForExit()
    $txt = Tool-Text $t
    if ($t.P.ExitCode -ne 0) { throw "import failed: $(($txt -split "`n")[-1])" }
    Say (($txt -split "`n") | Where-Object { $_ -match '^done' } | Select-Object -Last 1)
}

# the library's id: GAMELIB_ID-<the first 8 bytes of main's build ID, hex> (runtime/gamelib.h gamelib_full_id)
$hdr = [System.IO.File]::ReadAllBytes((Join-Path $Out 'exefs\main'))[0..0x5F]
if ([System.Text.Encoding]::ASCII.GetString($hdr, 0, 4) -ne 'NSO0') { throw "exefs\main isn't an NSO" }
$FullId = $GamelibId + '-' + (($hdr[0x40..0x47] | ForEach-Object { $_.ToString('X2') }) -join '')
Say "game code id $FullId"

# 2. the generator, every module at once
New-Item -ItemType Directory -Force $Work | Out-Null
$mods = @('main', 'sdk', 'subsdk0', 'subsdk1') | Where-Object { Test-Path (Join-Path $Out "exefs\$_") }
$gens = @{}
foreach ($m in $mods) {
    $dir = Join-Path $Work $m
    if (Test-Path (Join-Path $dir "${m}_table.c")) { continue }  # (done before: the table is written last)
    $argv = @($m, (Join-Path $Out "exefs\$m"), $Bases[$m], $dir)
    if ($m -eq 'sdk') { $argv += @('--hle', (Join-Path $Src 'hle_symbols.txt')) }
    $argv += @('--guest-shift', $Shift, '--simd', 'asm')
    $gens[$m] = Start-Tool (Join-Path $R 'lasgen\lasgen.exe') $argv
}
if ($gens.Count) { Say "generating C for $($gens.Keys -join ', ') ..." }
foreach ($m in @($gens.Keys)) {
    $gens[$m].P.WaitForExit()
    if ($gens[$m].P.ExitCode -ne 0) { throw "generating $m failed: $(Tool-Text $gens[$m])" }
}

# 3. compiling: zig's clang for aarch64-windows-gnu with what `zig cc` passes it (runtime/gamebuild.c compiler, ZIG_RAW)
$pre = @('clang', '--no-default-config', '-target', 'aarch64-unknown-windows-gnu', '-nostdinc', '-fhosted', '-DNDEBUG',
         '-D__MSVCRT_VERSION__=0xE00', '-D_WIN32_WINNT=0x0a00', '-D__CRT__NO_INLINE', '-fno-stack-protector')
foreach ($i in @('include', 'libc\include\aarch64-windows-gnu', 'libc\include\generic-mingw', 'libc\include\aarch64-windows-any',
                 'libc\include\any-windows-any')) { $pre += @('-isystem', (Join-Path $Lib $i)) }
# giant functions at -O0, as the start screen (gamebuild.c cold_giant: one function, over 1000 labels)
Add-Type -TypeDefinition @'
public static class PlaGiant {
    public static bool Is(string path) {  // (C#: PowerShell would read the GB of generated C line by line for minutes)
        int defs = 0; long labels = 0;
        foreach (var l in System.IO.File.ReadLines(path)) {
            if (l.StartsWith("L_", System.StringComparison.Ordinal)) labels++;
            else if (l.StartsWith("void f_", System.StringComparison.Ordinal) && l.Contains("(Cpu *c) {")) {
                if (++defs > 1) return false;
            }
        }
        return defs == 1 && labels > 1000;
    }
}
'@
function Is-Giant($path) { [PlaGiant]::Is($path) }
$units = @([pscustomobject]@{ Src = (Join-Path $Src 'gamelib_entry.c'); Obj = (Join-Path $Work 'gamelib_entry.o') })
foreach ($m in $mods) {
    Get-ChildItem (Join-Path $Work $m) -Filter '*.c' | Sort-Object Name | ForEach-Object {
        $units += [pscustomobject]@{ Src = $_.FullName; Obj = (Join-Path $Work ($_.BaseName + '.o')) }
    }
}
$todo = [System.Collections.Generic.Queue[object]]::new()
foreach ($u in $units) { if (-not (Test-Path $u.Obj)) { $todo.Enqueue($u) } }  # (objects of an interrupted run stay)
Say "compiling $($todo.Count) of $($units.Count) files, $Jobs at a time ..."
$run = @(); $done = $units.Count - $todo.Count; $last = Get-Date
while ($todo.Count -or $run.Count) {
    while ($todo.Count -and $run.Count -lt $Jobs) {
        $u = $todo.Dequeue()
        $giant = $u.Src -notmatch 'gamelib_entry' -and (Is-Giant $u.Src)
        $argv = $pre + ($CFlags | ForEach-Object { if ($giant -and $_ -like '-O*') { '-O0' } else { $_ } }) + @('-g0')
        if ($giant) { $argv += @('-fno-sanitize=all') }
        $argv += @("-I$Src", "-DGAMELIB_ID=`"$FullId`"", '-c', $u.Src, '-o', ($u.Obj + '.tmp'))
        $run += [pscustomobject]@{ U = $u; T = (Start-Tool $Zig $argv) }
    }
    Start-Sleep -Milliseconds 100
    $still = @()
    foreach ($job in $run) {  # (not $r: PowerShell's names ignore case - it would be $R, the folder)
        if (-not $job.T.P.HasExited) { $still += $job; continue }
        if ($job.T.P.ExitCode -ne 0) { throw "compiling $($job.U.Src) failed: $(Tool-Text $job.T)" }
        Move-Item -Force ($job.U.Obj + '.tmp') $job.U.Obj
        $done++
    }
    $run = $still
    if (((Get-Date) - $last).TotalSeconds -ge 15) { Say ("{0} / {1} files" -f $done, $units.Count); $last = Get-Date }
}

# 4. linking: lld with the runtime built ahead of time (zig\rt) and the exe's import library
$rsp = Join-Path $Work 'link.rsp'
[System.IO.File]::WriteAllLines($rsp, ($units | ForEach-Object { '"' + ($_.Obj -replace '\\', '/') + '"' }))
$rt = Join-Path $R 'zig\rt'
$dll = Join-Path $Out 'lasgame.dll'
Say "linking ..."
$t = Start-Tool $Zig @('lld-link', "@$(Join-Path $rt 'link.rsp')", "-LIBPATH:$rt", "-OUT:$dll.tmp", "@$rsp", (Join-Path $R 'PokeL-ARecomp.dll.a'))
$t.P.WaitForExit()
if ($t.P.ExitCode -ne 0) { throw "linking failed: $(Tool-Text $t)" }
Move-Item -Force "$dll.tmp" $dll
[System.IO.File]::WriteAllText("$dll.id", $FullId)
# (the device writes its own recomp_state.ini - its hardware - on the first start; one from here would lock it to this PC)
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $Out 'recomp_state.ini')
Remove-Item -Recurse -Force $Work
Say "done: $dll ($([Math]::Round((Get-Item $dll).Length / 1MB)) MB)"
Write-Host ""
Write-Host "Now copy this whole folder ($R) to the device (gamefiles\ can stay behind) and start PokeL-ARecomp.exe there."
