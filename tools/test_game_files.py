"""Game files: whose they are, deleting them, setting them up, compiling them - the real runtime/paths.c and
runtime/gamebuild.c functions (tools/test_game_files.c), on scratch folders with synthetic NSO headers. No marker or
folder name makes files ours: only what exefs/main (and any leftover NSO "main") is. Another game's files, files that
can't be identified, and anything mixed with them are never deleted, set up over or compiled; a setup is staged,
checked, then moved into place, and a cut-off setup leaves only its own staging folder behind. Every file that must
not change is hashed before and after. Linux / macOS (POSIX); run from anywhere: python3 tools/test_game_files.py"""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
if os.name == 'nt':
    print('game files test: POSIX only (the Windows build shares the code; run it on Linux or macOS)')
    sys.exit(0)
flags = []
deps = os.environ.get('DEPS')  # (a prefix with SDL3, as for make DEPS=)
if deps:
    flags = [f'-I{deps}/include', f'-L{deps}/lib', f'-Wl,-rpath,{deps}/lib', '-lSDL3']
elif shutil.which('pkg-config') and subprocess.run(['pkg-config', '--exists', 'sdl3']).returncode == 0:
    flags = shlex.split(subprocess.run(['pkg-config', '--cflags', '--libs', 'sdl3'], capture_output=True, text=True,
                                       check=True).stdout)
plat = 'macos' if sys.platform == 'darwin' else 'linux'
extra = ['-framework', 'CoreFoundation'] if plat == 'macos' else ['-pthread', '-ldl']


def nso(build_id):  # "NSO0", padding, the build ID at 0x40 (all a main module needs here)
    return b'NSO0' + bytes(0x3c) + bytes.fromhex(build_id) + bytes(0x38)


ARCEUS, VIOLET = nso('aee8f150dda1b5a8'), nso('709bfd6611529864')
ZA_202, LA_100, LA_101 = nso('b1f12fd919eae86a'), nso('ae16f71e002af8cb'), nso('909e904af78ac1b8')
UNKNOWN = nso('0123456789abcdef')
NAMES = {VIOLET: 'Pokémon Violet 4.0.0', ZA_202: 'Pokémon Legends: Z-A 2.0.2',
         LA_100: "The Legend of Zelda: Link's Awakening 1.0.0",
         LA_101: "The Legend of Zelda: Link's Awakening 1.0.1 / 1.1.0", UNKNOWN: 'an unknown build (0123456789abcdef)'}


def game(d, main, romfs='bin'):
    """A set-up game folder: exefs/ (main, main.npdm), the game data (romfs.bin or romfs/), compiled code, its cache"""
    d = Path(d)
    (d / 'exefs').mkdir(parents=True, exist_ok=True)
    (d / 'exefs/main').write_bytes(main)
    (d / 'exefs/main.npdm').write_bytes(b'npdm ' + main[0x40:0x48])
    if romfs == 'bin':
        (d / 'romfs.bin').write_bytes(b'romfs of ' + main[0x40:0x48])
    else:
        (d / 'romfs/arc').mkdir(parents=True, exist_ok=True)
        (d / 'romfs/arc/data.trpfs').write_bytes(b'packs of ' + main[0x40:0x48])
    (d / 'lasgame.so').write_bytes(b'game code of ' + main[0x40:0x48])
    (d / '.gamebuild').mkdir(exist_ok=True)
    (d / '.gamebuild/identity').write_text('cache\n')
    return d


def snapshot(top):
    """Every path under top: links (their target), folders, files (mode, size, mtime, SHA-256)"""
    top, out = Path(top), {}
    if not top.exists() and not top.is_symlink():
        return None
    for p in sorted([top] + list(top.rglob('*'))):
        st, key = p.lstat(), str(p.relative_to(top))
        if p.is_symlink():
            out[key] = ('link', os.readlink(p))
        elif p.is_dir():
            out[key] = ('dir', st.st_mode)
        else:
            out[key] = (st.st_mode, st.st_size, st.st_mtime_ns, hashlib.sha256(p.read_bytes()).hexdigest())
    return out


results = []


def check(cond, label):
    results.append(label)
    assert cond, label


with tempfile.TemporaryDirectory() as directory:
    T = Path(directory).resolve()
    exe = T / 'game-files-test'
    objs = []
    cc = shlex.split(os.environ.get('CC', 'clang'))
    cxx = shlex.split(os.environ.get('CXX', 'clang++'))
    inc = [f for f in flags if f.startswith('-I')]
    for src in ('runtime/paths.c', f'runtime/platform/{plat}.c', 'runtime/gamebuild.c', 'tools/test_game_files.c'):
        o = T / (Path(src).stem + '.o')
        subprocess.run(cc + ['-std=gnu11', '-Iruntime', '-DGAMELIB_ID="test"', '-DGAMELIB_CFLAGS="-O0"',
                             '-DGAMELIB_SHARED="-shared"', '-c', src, '-o', str(o)] + inc, cwd=ROOT, check=True)
        objs.append(str(o))
    subprocess.run(cxx + ['-std=c++17', '-Iruntime', '-c', 'runtime/build_cache.cpp', '-o', str(T / 'bc.o')] + inc,
                   cwd=ROOT, check=True)
    subprocess.run(cxx + objs + [str(T / 'bc.o'), '-o', str(exe)] + [f for f in flags if not f.startswith('-I')] + extra,
                   cwd=ROOT, check=True)

    # Each case runs in PokeL-ARecomp's own default data folder, <scratch HOME>/.local/share/plarecomp: the one folder
    # the app starts in whatever it holds (another app's folder is refused at start - tools/test_data_dir.py), so the
    # case where another game's files sit in extracted/ (PokeV-Recomp v0.0.24 and older set Violet up there) is real.
    env = {k: v for k, v in os.environ.items() if not k.startswith(('LARECOMP', 'XDG_DATA'))}
    env.update(SDL_VIDEO_DRIVER='dummy')

    def run_env(root):  # root = <home>/.local/share/plarecomp
        return dict(env, HOME=str(root.parents[2]))

    def run(root, *args, timeout=60):
        r = subprocess.run([str(exe), *map(str, args)], cwd=T, env=run_env(root), capture_output=True, text=True,
                           timeout=timeout)
        return r.returncode, r.stdout.strip(), r.stderr

    def out(root, *args):
        rc, o, e = run(root, *args)
        assert rc == 0, f'{args}: exit {rc}\n{e}'
        return o

    def fresh(name):
        if (T / name).exists():
            shutil.rmtree(T / name)
        r = T / name / 'home/.local/share/plarecomp'
        r.mkdir(parents=True)
        return r

    src_arceus = game(T / 'dump-arceus', ARCEUS, romfs='dir')   # (a player's extracted dump: exefs/ + romfs/)
    src_za = game(T / 'dump-za', ZA_202, romfs='dir')
    src_unknown = game(T / 'dump-unknown', UNKNOWN, romfs='dir')
    src_mixed = game(T / 'dump-mixed', ARCEUS, romfs='dir')
    (src_mixed / 'nso_raw').mkdir()
    (src_mixed / 'nso_raw/main').write_bytes(ZA_202)

    def refused_everywhere(label, root, main_name):
        """extracted/ there must be: owned by someone else, not deleted (whole or one entry), not set up over, not
        compiled - and byte for byte unchanged"""
        x = root / 'extracted'
        before = snapshot(x)
        o = out(root, 'owner', x)
        check(o.startswith('owner=foreign ' if main_name in NAMES.values() and 'unknown' not in main_name
                           else 'owner=unknown ') and main_name in o, f'{label}: identified as {main_name} ({o})')
        check(out(root, 'remove') == 'removed=0', f'{label}: delete refused')
        check(out(root, 'remove1', 'romfs.bin') == 'removed=0' and out(root, 'remove1', 'lasgame.so') == 'removed=0',
              f'{label}: single-entry delete refused')
        o = out(root, 'setup', src_arceus)
        check(o.startswith('begin=0 commit=0') and 'left as they are' in o, f'{label}: setup over it refused ({o[:60]})')
        o = out(root, 'build', x)
        check(o.startswith('built=0') and 'not compiled' in o, f'{label}: compile refused ({o[:70]})')
        check(snapshot(x) == before, f'{label}: extracted/ byte for byte unchanged')
        check(not (root / '.plarecomp-import').exists(), f'{label}: no staging folder left')
        print(f'{label}: refused (delete, setup, compile); unchanged')

    # ---- P1: the old marker after another app re-used the folder. PokeL-ARecomp (820a51b) set Arceus up and left
    # extracted/.plarecomp_setup; PokeV-Recomp v0.0.24 (same plarecomp/ folder) then set Violet up there: it removed
    # its GAME_FILES list, which doesn't know the marker, so rmdir failed, the marker stayed, and Violet went in.
    r = fresh('p1')
    x = game(r / 'extracted', ARCEUS)
    (x / '.plarecomp_setup').write_text('set up by PokeL-ARecomp\n')
    for name in ('exefs', 'romfs', 'romfs.bin', 'lasgame.so', 'lasgame.so.id', '.gamebuild'):  # (PokeV v0.0.24's list)
        p = x / name
        shutil.rmtree(p) if p.is_dir() else p.unlink(missing_ok=True)
    game(x, VIOLET)
    refused_everywhere('P1 stale marker + Violet (PokeV v0.0.24 over an earlier Arceus setup)', r, NAMES[VIOLET])

    # ---- P2: the marker as a folder, Violet's files with it
    r = fresh('p2')
    x = game(r / 'extracted', VIOLET)
    (x / '.plarecomp_setup').mkdir()
    refused_everywhere('P2 marker folder + Violet', r, NAMES[VIOLET])

    # ---- P4: Link's Awakening 1.0.0, and 1.0.1 / 1.1.0 (one main program: runtime/game_addresses/versions.h)
    for label, main in (('P4 Link\'s Awakening 1.0.1 / 1.1.0', LA_101), ('P4 Link\'s Awakening 1.0.0', LA_100)):
        r = fresh('p4')
        game(r / 'extracted', main)
        refused_everywhere(label, r, NAMES[main])

    # ---- P6: mixed contents - Arceus's exefs/main, and another game's main in a leftover folder
    r = fresh('p6')
    x = game(r / 'extracted', ARCEUS)
    (x / 'nso_raw').mkdir()
    (x / 'nso_raw/main').write_bytes(ZA_202)
    refused_everywhere('P6 mixed (Arceus exefs/main + Z-A nso_raw/main)', r, NAMES[ZA_202])
    # ...and a mixed dump picked for setup: staged, refused at the check, never moved in; the previous setup stays
    r = fresh('p6b')
    game(r / 'extracted', ARCEUS)
    before = snapshot(r / 'extracted')
    o = out(r, 'setup', src_mixed)
    check(o.startswith('begin=1 commit=0') and 'Z-A 2.0.2' in o, f'P6 mixed dump: refused at the check ({o[:70]})')
    check(snapshot(r / 'extracted') == before and not (r / '.plarecomp-import').exists(),
          'P6 mixed dump: previous setup unchanged, staging folder removed')
    print('P6 mixed dump picked for setup: refused before replacing anything')

    # ---- P8: an unknown build - never compiled, never deleted automatically
    r = fresh('p8')
    game(r / 'extracted', UNKNOWN)
    refused_everywhere('P8 unknown build', r, NAMES[UNKNOWN])
    r = fresh('p8b')
    game(r / 'extracted', ARCEUS)
    before = snapshot(r / 'extracted')
    o = out(r, 'setup', src_unknown)
    check(o.startswith('begin=1 commit=0') and '0123456789abcdef' in o, f'P8 unknown dump: refused at the check ({o})')
    check(snapshot(r / 'extracted') == before and not (r / '.plarecomp-import').exists(),
          'P8 unknown dump: previous setup unchanged, staging folder removed')
    for label, (names, extra_files) in {'romfs.bin only': ([], ['romfs.bin']),
                                         'exefs/ without main': (['exefs'], []),
                                         'exefs/main not an NSO': (['exefs'], ['exefs/main'])}.items():
        r = fresh('p8c')
        for d in names:
            (r / 'extracted' / d).mkdir(parents=True)
        for f in extra_files:
            (r / 'extracted' / f).parent.mkdir(parents=True, exist_ok=True)
            (r / 'extracted' / f).write_bytes(b'dummy')
        before = snapshot(r / 'extracted')
        check(out(r, 'owner', r / 'extracted').startswith('owner=unknown') and out(r, 'remove') == 'removed=0' and
              snapshot(r / 'extracted') == before, f'P8 unidentifiable ({label}): not deleted')
    print('P8 unknown and unidentifiable files: never deleted, compiled or set up over')

    # ---- the wrong dump the marker was for (a Z-A dump picked by mistake): no marker needed - it never reaches
    # extracted/, and the previous setup stays as it was
    r = fresh('wrong')
    game(r / 'extracted', ARCEUS)
    before = snapshot(r / 'extracted')
    o = out(r, 'setup', src_za)
    check(o.startswith('begin=1 commit=0') and 'Z-A 2.0.2' in o, f'wrong dump: refused at the check ({o})')
    check(snapshot(r / 'extracted') == before and not (r / '.plarecomp-import').exists(),
          'wrong dump: previous setup unchanged, staging folder removed')
    o = out(r, 'setup-fail', src_arceus)  # (an import that fails: zaimport's error)
    check(o == 'begin=1 commit=0 aborted' and snapshot(r / 'extracted') == before and
          not (r / '.plarecomp-import').exists(), 'failed import: staging folder removed, setup unchanged')
    print('wrong dump / failed import: never reach extracted/; previous setup unchanged')

    # ---- P7: a setup cut off (the process dies while files are staged), then the app starts again
    r = fresh('p7')
    game(r / 'extracted', ARCEUS)
    other = game(T / 'p7-foreign', ZA_202)  # (another app's files elsewhere: must not change either)
    before, before_other = snapshot(r / 'extracted'), snapshot(other)
    rc, o, _ = run(r, 'setup-crash', src_arceus)
    check(rc == 9 and (r / '.plarecomp-import').is_dir(), 'P7 cut-off setup: staging folder left behind')
    check(snapshot(r / 'extracted') == before, 'P7 cut-off setup: extracted/ unchanged')
    out(r, 'init')  # (the restart)
    check(not (r / '.plarecomp-import').exists(), 'P7 restart: staging folder removed')
    check(snapshot(r / 'extracted') == before and snapshot(other) == before_other,
          'P7 restart: extracted/ and the other folder unchanged')
    # a staging folder that is a link to someone else's files: only the link goes
    (r / '.plarecomp-import').symlink_to(other)
    out(r, 'init')
    check(not (r / '.plarecomp-import').is_symlink() and snapshot(other) == before_other,
          'P7 staging link: only the link removed, its target unchanged')
    # another copy of the app setting up right now: its staging folder is left alone, and a second setup waits
    holder = subprocess.Popen([str(exe), 'setup-hold', str(src_arceus), '4'], cwd=T, stdout=subprocess.PIPE, text=True,
                              env=run_env(r))
    line = holder.stdout.readline()
    check(line.startswith('begin=1 holding'), f'P7 lock: first setup staged ({line.strip()})')
    out(r, 'init')
    check((r / '.plarecomp-import').is_dir(), 'P7 lock: a restart leaves a running setup\'s staging folder alone')
    o = out(r, 'setup', src_arceus)
    check(o.startswith('begin=0') and 'Another copy' in o, f'P7 lock: a second setup is refused ({o})')
    holder.wait(timeout=30)
    check(not (r / '.plarecomp-import').exists() and snapshot(r / 'extracted') == before,
          'P7 lock: the first setup cleaned up after itself; extracted/ unchanged')
    print('P7 cut-off setup: only its staging folder removed at the next start; a running setup is left alone')

    # ---- the normal way (P9 for the game files): set up, set up again over our own files, delete, compile check
    r = fresh('ok')
    o = out(r, 'setup', src_arceus)
    check(o.startswith('begin=1 commit=1'), f'setup: fresh data folder ({o})')
    check((r / 'extracted/exefs/main').read_bytes() == ARCEUS and (r / 'extracted/romfs/arc/data.trpfs').exists() and
          not (r / '.plarecomp-import').exists(), 'setup: files in place, staging folder gone')
    (r / 'extracted/lasgame.so').write_bytes(b'old code')
    o = out(r, 'setup', src_arceus)
    check(o.startswith('begin=1 commit=1') and (r / 'extracted/lasgame.so').read_bytes() != b'old code',
          f'setup again over our own files: replaced ({o})')
    shutil.rmtree(r / 'extracted/.gamebuild')  # (the fake dump's own; the check must not make one)
    o = out(r, 'build', r / 'extracted')
    check(o.startswith('built=0') and 'builder is missing' in o and not (r / 'extracted/.gamebuild').exists(),
          f'compile: our own files pass the check (and stop at the missing builder here) ({o[:70]})')
    (r / 'extracted/mine.txt').write_text('the player\'s note')
    o = out(r, 'setup', src_arceus)
    check(o.startswith('begin=0') and 'mine.txt' in o and (r / 'extracted/exefs/main').exists(),
          f'setup over a folder with the player\'s own file: refused before deleting anything ({o[:60]})')
    (r / 'extracted/mine.txt').unlink()
    check(out(r, 'remove1', 'lasgame.so') == 'removed=1', 'delete one entry of our own files')
    check(out(r, 'remove') == 'removed=1' and not (r / 'extracted').exists(), 'delete our own files: all gone')
    # extracted/ as a link (a dev worktree's): never set up through
    (T / 'elsewhere').mkdir()
    (r / 'extracted').symlink_to(T / 'elsewhere')
    o = out(r, 'setup', src_arceus)
    check(o.startswith('begin=0') and not any((T / 'elsewhere').iterdir()), f'setup through a linked extracted/ refused')
    print('normal setup, set up again, delete, compile check: as before')

print(f'game files: {len(results)} checks passed - another game\'s, unidentified and mixed files are never deleted, '
      'set up over or compiled; setups are staged and checked; cut-off setups leave only their staging folder')
