"""Data folder isolation (runtime/paths.c, Linux).

PokeL-ARecomp's data root is $XDG_DATA_HOME/plarecomp (~/.local/share/plarecomp), and never another recomp's folder,
however it is named as the data root: LARECOMP_DATA_ROOT, LARECOMP_DATA_HOME, a data_root.txt, or dev mode (started
from a folder with extracted/). Another app's folder is recognised by its name (lasrecomp, zarecomp, pokev-recomp),
by another game's build in its extracted/, or by its settings / log naming LAS Recomp or PokeV-Recomp - with or
without an extracted/ - and refused before anything is written there. Builds tools/test_data_dir.c with the real
paths.c and platform/linux.c and runs it under scratch HOME / XDG_DATA_HOME values next to fake pokev-recomp/,
zarecomp/ and lasrecomp/ folders, which must stay byte for byte as they were. Scratch folders only: the player's own
data is never read."""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
if not sys.platform.startswith('linux'):
    print('data dir test: Linux only (Windows keeps its data next to the .exe)')
    sys.exit(0)
flags = []
deps = os.environ.get('DEPS')  # (a prefix with SDL3, as for make DEPS=)
if deps:
    flags = [f'-I{deps}/include', f'-L{deps}/lib', f'-Wl,-rpath,{deps}/lib', '-lSDL3']
elif shutil.which('pkg-config') and subprocess.run(['pkg-config', '--exists', 'sdl3']).returncode == 0:
    flags = shlex.split(subprocess.run(['pkg-config', '--cflags', '--libs', 'sdl3'], capture_output=True, text=True,
                                       check=True).stdout)


def snapshot(top):
    """Every path under top: files and links with their mode, size, mtime and contents' hash (or link target); folders
    by mode only (rt_paths_init's write check creates and removes a .write_test file in the data root, which moves a
    folder's mtime - never in a refused folder: there nothing at all may change)."""
    out = {}
    for p in sorted(top.rglob('*')):
        st = p.lstat()
        key = str(p.relative_to(top))
        if p.is_symlink():
            out[key] = ('link', os.readlink(p))
        elif p.is_dir():
            out[key] = (st.st_mode,)
        else:
            out[key] = (st.st_mode, st.st_size, st.st_mtime_ns, hashlib.sha256(p.read_bytes()).hexdigest())
    return out


def strict(top):
    """A refused folder: every entry, folders' mtimes too (nothing created, renamed or removed in it)"""
    return {k: v + ((top / k).lstat().st_mtime_ns,) for k, v in snapshot(top).items()} | {'.': top.stat().st_mtime_ns}


def nso(build_id):
    return b'NSO0' + bytes(0x3c) + bytes.fromhex(build_id) + bytes(0x38)


VIOLET, ZA, LA, ARCEUS = nso('709bfd6611529864'), nso('b1f12fd919eae86a'), nso('909e904af78ac1b8'), nso('aee8f150dda1b5a8')
SIBLINGS = {'pokev-recomp': (VIOLET, '# PokeV-Recomp settings (edited from the in-game menu)\n', '[info] PokeV-Recomp v0.0.27'),
            'zarecomp': (ZA, '# ZA Recomp settings (edited from the in-game menu: F1)\n', '[info] ZA Recomp v0.2.0'),
            'lasrecomp': (LA, '# LAS Recomp settings (edited from the in-game menu)\n', '[info] LAS Recomp v1.2.0')}
checks = 0

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory).resolve()
    exe = directory / 'data-dir-test'
    subprocess.run(shlex.split(os.environ.get('CC', 'clang')) +
                   ['-std=gnu11', '-Iruntime', 'runtime/paths.c', 'runtime/platform/linux.c', 'tools/test_data_dir.c',
                    '-o', str(exe)] + flags + ['-pthread', '-ldl'], cwd=ROOT, check=True)

    home = directory / 'home'
    share = home / '.local/share'
    # the other recomps' data folders, as a player with all of them installed has them - including the old marker
    # (extracted/.plarecomp_setup) the earlier version of this change wrote, which must count for nothing
    for name, (main, ini, info) in SIBLINGS.items():
        d = share / name
        (d / 'extracted/exefs').mkdir(parents=True)
        (d / 'extracted/exefs/main').write_bytes(main)
        (d / 'extracted/romfs.bin').write_bytes(b'romfs')
        (d / 'extracted/.gamebuild').mkdir()
        (d / 'extracted/.plarecomp_setup').write_text('set up by PokeL-ARecomp\n')
        (d / 'save/save_0').mkdir(parents=True)
        (d / 'save/save_0/main').write_bytes(b'save of ' + name.encode())
        (d / 'larecomp.ini').write_text(ini + 'fps60 = 1\n')
        (d / 'lasrecomp.log').write_text(info + ', Bazzite 45\n[paths] data: ' + str(d) + '\n')
        (d / 'data_root.txt').write_text(str(d) + '\n')
    before = snapshot(share)
    start = directory / 'start'  # (where the AppImage is started from: no extracted/ there, so not dev mode)
    start.mkdir()
    clean = {k: v for k, v in os.environ.items()
             if k not in ('XDG_DATA_HOME', 'LARECOMP_DATA_ROOT', 'LARECOMP_DATA_HOME', 'LARECOMP_SETTINGS')}
    clean.update(HOME=str(home), SDL_VIDEO_DRIVER='dummy')

    def run(mode, cwd=start, want=0, **env):
        r = subprocess.run([str(exe), mode], cwd=cwd, env=dict(clean, **env), capture_output=True, text=True)
        assert r.returncode == want, f'{mode} {env}: exit {r.returncode}\n{r.stdout}{r.stderr}'
        return r.stdout.split('\n') if want == 0 else r.stderr

    def siblings_unchanged(why):
        now = {k: v for k, v in snapshot(share).items() if not k.startswith('plarecomp')}
        assert now == before, f'another recomp\'s data folder changed ({why})'

    def check(cond, label):
        global checks
        checks += 1
        assert cond, label

    # ---- P9: PokeL-ARecomp's own folder. XDG_DATA_HOME, when absolute, is the parent; unset, empty or relative:
    # ~/.local/share
    want = share / 'plarecomp'
    xdg = directory / 'xdg'
    check(run('early')[0] == str(want), 'P9 default: ~/.local/share/plarecomp')
    check(run('early', XDG_DATA_HOME=str(xdg))[0] == str(xdg / 'plarecomp'), 'P9 absolute XDG_DATA_HOME')
    check(run('early', XDG_DATA_HOME='relative/dir')[0] == str(want), 'P9 relative XDG_DATA_HOME ignored')
    check(run('early', XDG_DATA_HOME='')[0] == str(want), 'P9 empty XDG_DATA_HOME ignored')
    root, cwd = run('init', XDG_DATA_HOME=str(xdg))[:2]
    check(root == cwd == str(xdg / 'plarecomp'), f'P9 absolute XDG_DATA_HOME: init ({root})')
    root, cwd = run('init')[:2]
    check(root == cwd == str(want) and want.is_dir() and not (want / 'extracted').exists(), 'P9 default: init, fresh')
    check(not (want / 'larecomp.ini').exists(), 'P9 no other app\'s settings copied in')
    check(run('init')[0] == str(want), 'P9 a second start uses the same folder')
    siblings_unchanged('fresh start')
    print('P9 own folder: $XDG_DATA_HOME/plarecomp when absolute, else ~/.local/share/plarecomp; siblings untouched')

    # ---- P3 / P5 and the other overrides: another app's folder, however it is named as the data root, is refused at
    # start - before the session log, settings or a write test touch it - and stays byte for byte as it was
    def refused(label, folder, reason, cwd=start, early=True, **env):
        snap = strict(folder)
        err = run('init', cwd=cwd, want=1, **env)
        check("won't use this folder" in err and reason in err and 'Nothing in it was changed' in err,
              f'{label}: refused with the reason ({err.strip().splitlines()[-3:]})')
        err = run('remove', cwd=cwd, want=1, **env)
        if early:
            e = run('early', cwd=cwd, **env)[0]
            check(e in ('none', str(want)), f'{label}: no session log in it ({e})')
        check(strict(folder) == snap, f'{label}: byte for byte unchanged, nothing created')
        print(f'{label}: refused at start, unchanged')

    za = share / 'zarecomp'
    refused('P3 LARECOMP_DATA_ROOT=zarecomp (Z-A files + old marker)', za, "ZA Recomp's data folder",
            LARECOMP_DATA_ROOT=str(za))
    refused('dev mode inside pokev-recomp/', share / 'pokev-recomp', "PokeV-Recomp's data folder",
            cwd=share / 'pokev-recomp', early=False)
    (want / 'data_root.txt').write_text(str(share / 'lasrecomp') + '\n')
    refused('data_root.txt -> lasrecomp/', share / 'lasrecomp', "LAS Recomp's data folder")
    (want / 'data_root.txt').unlink()
    refused('LARECOMP_DATA_HOME=zarecomp', za, "ZA Recomp's data folder", LARECOMP_DATA_HOME=str(za))
    siblings_unchanged('overrides')

    # P5: no extracted/ - saves and settings only; empty; renamed copies recognised by what they hold
    p5 = directory / 'p5'
    cases = {
        'P5 zarecomp/ with saves + settings, no extracted/': ('zarecomp', None, '# ZA Recomp settings\n', None,
                                                             "ZA Recomp's data folder"),
        'P5 empty zarecomp/': ('zarecomp', None, None, None, "ZA Recomp's data folder"),
        'P5 empty "PokeV-Recomp" (a data move\'s folder)': ('PokeV-Recomp', None, None, None, "PokeV-Recomp's data folder"),
        'P5 renamed copy, PokeV-Recomp settings, no extracted/': ('my-saves', None, SIBLINGS['pokev-recomp'][1], None,
                                                                 "settings file (larecomp.ini) is PokeV-Recomp's"),
        'P5 renamed copy, LAS Recomp log, no extracted/': ('other-copy', None, None, '[info] LAS Recomp v1.2.0, Linux\n',
                                                          "log (lasrecomp.log) is LAS Recomp's"),
        'P5 renamed copy, Link\'s Awakening 1.0.1 game files': ('games-x', LA, None, None,
                                                                "Link's Awakening 1.0.1 / 1.1.0's game files"),
    }
    for i, (label, (name, main, ini, log, reason)) in enumerate(cases.items()):
        d = p5 / str(i) / name
        d.mkdir(parents=True)
        (d / 'save/save_0').mkdir(parents=True)
        (d / 'save/save_0/main').write_bytes(b'irreplaceable save')
        if main:
            (d / 'extracted/exefs').mkdir(parents=True)
            (d / 'extracted/exefs/main').write_bytes(main)
        if ini:
            (d / 'larecomp.ini').write_text(ini)
        if log:
            (d / 'lasrecomp.log').write_text(log)
        if 'empty' in label:
            shutil.rmtree(d / 'save')
        refused(label, d, reason, LARECOMP_DATA_ROOT=str(d))

    # ---- P9: legitimate folders keep working - data_root.txt to a folder of the player's, a scratch
    # LARECOMP_DATA_ROOT (its settings: the shipped za-default.ini's "# ZA Recomp settings" header, as a fresh
    # PokeL-ARecomp install has it), dev mode in a source checkout's data folder with Arceus's files
    mine = directory / 'Games/PokeL-ARecomp data'
    mine.mkdir(parents=True)
    (want / 'data_root.txt').write_text(str(mine) + '\n')
    check(run('init')[0] == str(mine) and run('early')[0] == str(mine), 'P9 data_root.txt to the player\'s folder')
    (want / 'data_root.txt').unlink()
    scratch = directory / 'scratch-root'
    scratch.mkdir()
    (scratch / 'larecomp.ini').write_text('# ZA Recomp settings (edited from the in-game menu: F1)\nfps60 = 0\n')
    (scratch / 'lasrecomp.log').write_text('[info] PokeL-ARecomp v0.26.7, Linux\n')
    check(run('init', LARECOMP_DATA_ROOT=str(scratch))[0] == str(scratch), 'P9 scratch LARECOMP_DATA_ROOT')
    dev = directory / 'checkout'
    (dev / 'extracted/exefs').mkdir(parents=True)
    (dev / 'extracted/exefs/main').write_bytes(ARCEUS)
    check(run('init', cwd=dev)[0] == str(dev) and run('early', cwd=dev)[0] == 'none', 'P9 dev mode in a checkout')
    print('P9 legitimate folders: data_root.txt, a scratch LARECOMP_DATA_ROOT, dev mode - all used')

    # PokeL-ARecomp's own default folder always starts - even with another app's files in it (Violet's, from
    # PokeV-Recomp v0.0.24 and older) - and those files are never deleted
    game = want / 'extracted'
    (game / 'exefs').mkdir(parents=True)
    (game / 'exefs/main').write_bytes(VIOLET)
    (game / '.plarecomp_setup').write_text('old marker\n')
    out = run('remove')
    check(out[0] == str(want) and out[1].startswith('owner=2 Pokémon Violet 4.0.0') and out[2] == 'removed=0', out)
    check((game / 'exefs/main').read_bytes() == VIOLET, 'own folder: Violet kept')
    shutil.rmtree(game)
    # a symlinked extracted/ in it, to another app's: never deleted through
    game.symlink_to(share / 'pokev-recomp/extracted')
    check(run('remove')[2] == 'removed=0', 'symlinked extracted/: refused')
    game.unlink()
    print('own folder: starts with another app\'s files in it, which stay; a symlinked extracted/ is never deleted through')

    siblings_unchanged('end')
    print(f'linux data folder: {checks} checks passed - pokev-recomp/, zarecomp/, lasrecomp/ and every refused folder '
          'byte for byte unchanged')
