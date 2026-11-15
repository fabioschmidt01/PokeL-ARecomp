"""Game identity (runtime/game_identity.h): only Pokémon Legends: Arceus 1.1.1 is accepted for setup and compilation;
Violet, Z-A and Link's Awakening files are refused by name. Also checks that the start screen and main.c take their
answer from that header (no build table of their own), that the supported build agrees with the generator's
(tools/pla_patches.py PLA_BUILDS), and that every compilation and every setup deletion is preceded by the identity
checks. Fake NSO headers in a scratch folder; no game files used."""
import ast
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    exe = directory / ('game-identity-test.exe' if os.name == 'nt' else 'game-identity-test')
    for cc, std in ((os.environ.get('CC', 'clang'), '-std=gnu11'), (os.environ.get('CXX', 'clang++'), '-std=gnu++17')):
        src = ['tools/test_game_identity.c'] if std.endswith('11') else ['-x', 'c++', 'tools/test_game_identity.c']
        subprocess.run(shlex.split(cc) + [std, '-Wall', '-Werror', '-Iruntime'] + src + ['-o', str(exe)],
                       cwd=ROOT, check=True)
        scratch = directory / 'scratch'
        scratch.mkdir(exist_ok=True)
        run = subprocess.run([str(exe), str(scratch)], capture_output=True, text=True)
        print(run.stderr + run.stdout, end='')
        assert run.returncode == 0 and 'all checks passed' in run.stdout, f'game identity test failed ({std})'

# The supported build must be the generator's (tools/pla_patches.py PLA_BUILDS: what gen.py patches as Arceus)
pla = (ROOT / 'tools/pla_patches.py').read_text()
header = (ROOT / 'runtime/game_identity.h').read_text()
supported = re.findall(r'\{\{([^}]*)\}, "([^"]*)", "([^"]*)", true\}', header)
assert len(supported) == 1, f'one supported build expected in game_identity.h, found {supported}'
ids, game, version = supported[0]
arceus = ''.join(b.strip()[2:] for b in ids.split(','))
m = re.search(r'^PLA_BUILDS = (\{[^}]*\})', pla, re.M)
assert m, 'PLA_BUILDS not found in tools/pla_patches.py'
assert ast.literal_eval(m.group(1)) == {arceus: version}, f'{arceus} {version} is not tools/pla_patches.py PLA_BUILDS'
assert game == 'Pok\\xc3\\xa9mon Legends: Arceus', game

# The start screen and main.c take the answer from the header: no build table of their own that could let another
# game's files through again (until v0.26.7 the start screen accepted Link's Awakening and Z-A builds as set up)
launcher = (ROOT / 'runtime/ui/launcher.mm').read_text()
main = (ROOT / 'runtime/main.c').read_text()
assert 'return game_identity_supported(id);' in launcher
assert 'game_identity_supported(id)' in main
for src, name in ((launcher, 'launcher.mm'), (main, 'main.c')):
    for other in ('0xae, 0xe8, 0xf1, 0x50', '0x72, 0x22, 0xe1, 0x3e', '0x8a, 0x1c, 0x86, 0xc4', '0xb1, 0xf1, 0x2f, 0xd9',
                  '0xae, 0x16, 0xf7, 0x1e', '0x70, 0x9b, 0xfd, 0x66'):
        assert other not in src, f'{name} still lists a build id of its own ({other})'

# Every compilation goes through build_game_code, which checks the identity before gamebuild_run
assert launcher.count('gamebuild_run(') == 1, 'gamebuild_run has more than one caller in launcher.mm'
callers = [p for p in (ROOT / 'runtime').rglob('*') if p.suffix in ('.c', '.mm', '.cpp') and p.name != 'gamebuild.c'
           and 'gamebuild_run(' in p.read_text(errors='replace')]
assert [p.name for p in callers] == ['launcher.mm'], callers
body = launcher[launcher.index('static bool build_game_code(void)'):]
assert body.index('build_id_ok(g_root + "/exefs/main")') < body.index('gamebuild_run('), \
    'build_game_code must check the identity before gamebuild_run'

# The setup thread refuses another app's files before it deletes, imports over or compiles anything
setup = launcher[launcher.index('static void *extract_thread(void *)'):]
setup = setup[:setup.index('\ncheck:')]
guard = setup.index('rt_game_files_foreign(g_root.c_str()')
for what in ('rt_remove_game_files(', 'build_game_code()', 'import_packages('):
    assert guard < setup.index(what), f'extract_thread: {what} before the foreign-files check'
print('only Pokémon Legends: Arceus 1.1.1 is set up and compiled; Violet / Z-A / Link\'s Awakening files are refused '
      'by name, before any delete or compile')
