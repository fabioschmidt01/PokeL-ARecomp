"""Data folder isolation and the game-files delete guard (runtime/paths.c, Linux).

PokeL-ARecomp's data root is $XDG_DATA_HOME/plarecomp (~/.local/share/plarecomp), never another recomp's. Builds
tools/test_data_dir.c with the real paths.c and platform/linux.c and runs it under scratch HOME / XDG_DATA_HOME values
next to fake pokev-recomp/, zarecomp/ and lasrecomp/ folders holding Violet, Z-A and Link's Awakening game files, which
must stay byte for byte as they were - also when one of them is made PokeL-ARecomp's data root (LARECOMP_DATA_ROOT, a
data_root.txt, dev mode) or reached through a symlinked extracted/. Synthetic NSO headers in scratch folders only: the
player's own data and game files are never read."""
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


def nso(build_id):
    return b'NSO0' + bytes(0x3c) + bytes.fromhex(build_id) + bytes(0x38)


ARCEUS = nso('aee8f150dda1b5a8')
VIOLET = nso('709bfd6611529864')
ZA_202 = nso('b1f12fd919eae86a')
LA_100 = nso('ae16f71e002af8cb')
UNKNOWN = nso('0123456789abcdef')
SIBLINGS = {'pokev-recomp': (VIOLET, 'Pokémon Violet 3.0.1'),
            'zarecomp': (ZA_202, 'Pokémon Legends: Z-A 2.0.2'),
            'lasrecomp': (LA_100, "The Legend of Zelda: Link's Awakening 1.0.0")}


def snapshot(top):
    """Every path under top: files and links with their mode, size, mtime and contents' hash (or link target); folders
    by mode only (rt_paths_init's write check creates and removes a .write_test file in the data root, which moves a
    folder's mtime)."""
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


def game_files(d, main):
    """A set-up extracted/ as the start screen leaves it: exefs/main, romfs.bin, build cache and the game code"""
    (d / 'exefs').mkdir(parents=True, exist_ok=True)
    (d / 'exefs/main').write_bytes(main)
    (d / 'romfs.bin').write_bytes(b'romfs')
    (d / '.gamebuild').mkdir(exist_ok=True)
    (d / 'lasgame.so').write_bytes(b'game code')


with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory).resolve()
    exe = directory / 'data-dir-test'
    subprocess.run(shlex.split(os.environ.get('CC', 'clang')) +
                   ['-std=gnu11', '-Iruntime', 'runtime/paths.c', 'runtime/platform/linux.c', 'tools/test_data_dir.c',
                    '-o', str(exe)] + flags + ['-pthread', '-ldl'], cwd=ROOT, check=True)

    home = directory / 'home'
    share = home / '.local/share'
    # the other recomps' data folders, as a player with all of them installed has them
    for name, (main, _) in SIBLINGS.items():
        d = share / name
        game_files(d / 'extracted', main)
        (d / 'save/save_0').mkdir(parents=True)
        (d / 'save/save_0/main').write_bytes(b'save of ' + name.encode())
        (d / 'larecomp.ini').write_text('fps60 = 1\n')
        (d / 'data_root.txt').write_text(str(d) + '\n')
    before = snapshot(share)
    start = directory / 'start'  # (where the AppImage is started from: no extracted/ there, so not dev mode)
    start.mkdir()
    base_env = {k: v for k, v in os.environ.items()
                if k not in ('XDG_DATA_HOME', 'LARECOMP_DATA_ROOT', 'LARECOMP_DATA_HOME', 'LARECOMP_SETTINGS')}
    base_env.update(HOME=str(home), SDL_VIDEO_DRIVER='dummy')

    def run(mode, cwd=start, want_exit=0, **env):
        r = subprocess.run([str(exe), mode], cwd=cwd, env=dict(base_env, **env), capture_output=True, text=True)
        assert r.returncode == want_exit, f'{mode}: exit {r.returncode}\n{r.stderr}'
        return r.stdout.split('\n') if want_exit == 0 else r.stderr

    def siblings_unchanged(why):
        now = {k: v for k, v in snapshot(share).items() if not k.startswith('plarecomp')}
        assert now == before, f'another recomp\'s data folder changed ({why})'

    # 1-3. Where the data root is: XDG_DATA_HOME, when absolute, is the parent; unset, empty or relative: ~/.local/share
    want = share / 'plarecomp'
    assert run('early')[0] == str(want), run('early')
    xdg = directory / 'xdg'
    assert run('early', XDG_DATA_HOME=str(xdg))[0] == str(xdg / 'plarecomp')
    assert run('early', XDG_DATA_HOME='relative/dir')[0] == str(want)
    assert run('early', XDG_DATA_HOME='')[0] == str(want)
    root, cwd = run('init', XDG_DATA_HOME=str(xdg))[:2]
    assert root == cwd == str(xdg / 'plarecomp'), (root, cwd)
    print('data root: $XDG_DATA_HOME/plarecomp when absolute, else ~/.local/share/plarecomp')

    # 4-5. A fresh start makes its own folder, next to the others, and leaves them as they were
    root, cwd = run('init')[:2]
    assert root == str(want) and cwd == str(want), (root, cwd)
    assert want.is_dir() and not (want / 'extracted').exists(), 'a fresh data folder has no game files'
    assert not (want / 'larecomp.ini').exists(), 'another app\'s settings were copied'
    assert run('init')[0] == str(want), 'a second start uses the same folder'
    siblings_unchanged('fresh start')
    print('isolation: pokev-recomp/, zarecomp/, lasrecomp/ untouched by a fresh start')

    # 9-10. An override that makes another app's folder the data root (its extracted/ holds that app's game files) is
    # refused before anything is written there: no settings, logs or caches, and so no setup, delete or compile either
    def refused(label, name, cwd=start, **env):
        main, what = SIBLINGS[name]
        for mode in ('init', 'remove', 'mark'):
            err = run(mode, cwd=cwd, want_exit=1, **env)
            assert "won't use this folder" in err and f"It holds {what}'s game files" in err, (label, err)
        assert run('early', cwd=cwd, **env)[0] in ('none', str(want)), (label, 'session log in another app\'s folder')
        siblings_unchanged(label)
        print(f'override: {label} -> {name}/ ({what}) refused at start, nothing written there')

    refused('dev mode (started from inside it)', 'pokev-recomp', cwd=share / 'pokev-recomp')
    refused('LARECOMP_DATA_ROOT', 'zarecomp', LARECOMP_DATA_ROOT=str(share / 'zarecomp'))
    (want / 'data_root.txt').write_text(str(share / 'lasrecomp') + '\n')
    refused('data_root.txt', 'lasrecomp')
    (want / 'data_root.txt').unlink()

    # A symlinked extracted/ in our own folder, pointing at another app's: neither deleted through nor marked
    (want / 'extracted').symlink_to(share / 'pokev-recomp/extracted')
    out = run('remove')
    assert out[2] == 'removed=0 one=0' and run('mark')[1] == 'marked=0', out
    (want / 'extracted').unlink()
    siblings_unchanged('symlinked extracted/')
    print('delete guard: a symlinked extracted/ is neither deleted through nor marked')

    # 6-8 in our own folder: Arceus's own files and unknown builds are removed as before; Violet's left there by
    # PokeV-Recomp v0.0.24 and older (no setup mark) are refused; a wrong dump this app imported itself (marked) is ours
    game = want / 'extracted'
    for label, main, marked, foreign in (('Arceus 1.1.1', ARCEUS, False, False),
                                         ('unknown build', UNKNOWN, False, False),
                                         ('Violet, left by PokeV-Recomp', VIOLET, False, True),
                                         ('Z-A, imported by this app (marked)', ZA_202, True, False)):
        game_files(game, main)
        if marked:
            (game / '.plarecomp_setup').write_text('set up by PokeL-ARecomp\n')
        out = run('remove')
        assert out[1].startswith(f'foreign={int(foreign)}'), (label, out)
        assert out[2] == f'removed={int(not foreign)} one={int(not foreign)}', (label, out)
        assert (game / 'exefs/main').exists() == foreign and (game / 'lasgame.so').exists() == foreign, label
        assert not (game / '.plarecomp_setup').exists(), label
        if foreign:
            shutil.rmtree(game)  # (the test's own copy)
        print(f'own data folder: {label} -> {"refused" if foreign else "removed"}')
    game.mkdir()
    assert run('mark')[1] == 'marked=1' and (game / '.plarecomp_setup').is_file(), 'setup marks its own folder'
    shutil.rmtree(game)

    # PokeL-ARecomp's own folder is never refused at start, even with another app's files in it (Violet's, left in
    # plarecomp/ by PokeV-Recomp v0.0.24 and older): the start screen refuses to set up over them instead
    game_files(game, VIOLET)
    assert run('init')[0] == str(want) and run('remove')[2] == 'removed=0 one=0'
    assert (game / 'exefs/main').read_bytes() == VIOLET
    shutil.rmtree(game)
    print('own data folder: starts with another app\'s files in it, which stay')

    siblings_unchanged('end')
    print('linux: data in ~/.local/share/plarecomp; pokev-recomp/, zarecomp/, lasrecomp/ byte for byte unchanged; '
          'another app\'s game files never deleted (dev mode, LARECOMP_DATA_ROOT, data_root.txt, symlink)')
