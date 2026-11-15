"""Game identity (runtime/game_identity.h): only Pokémon Legends: Arceus 1.1.1 is accepted for setup and compilation;
Violet, Z-A and Link's Awakening (1.0.0, 1.0.1 / 1.1.0) are refused by name, unknown builds refused.

1. tools/test_game_identity.c, built as C and C++ (-Wall -Werror), reads synthetic NSO headers with the header's real
   functions (behaviour).
2. Every build ID in the table is checked against the project's own sources, so none can be mistyped or made up:
   PLA_BUILDS (tools/pla_patches.py), ZA_BUILDS (tools/za_patches.py), GA_VERSION (runtime/game_addresses/versions.h,
   Link's Awakening, as in LAS Recomp). Violet's comes from PokeV-Recomp (tools/sv_patches.py SV_BUILDS), not in this
   repository: it is pinned here with that source.
3. Structural checks on the source (not behaviour - tools/test_game_files.py runs that): the start screen and main.c
   keep no build table of their own, and the setup goes through the staging transaction.
Fake NSO headers in a scratch folder; no game files used."""
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

# 2. the table against the project's own sources
header = (ROOT / 'runtime/game_identity.h').read_text()
rows = [(''.join(b.strip()[2:] for b in ids.split(',')), game, version, supported == 'true')
        for ids, game, version, supported in
        re.findall(r'\{\{([^}]*)\}, "([^"]*)", "([^"]*)", (true|false)\}', header)]
table = {r[0]: r for r in rows}
assert len(table) == len(rows), 'a build ID is listed twice in game_identity.h'


def literal(path, name):
    m = re.search(rf'^{name} = (\{{[^}}]*\}})', (ROOT / path).read_text(), re.M)
    assert m, f'{name} not found in {path}'
    return ast.literal_eval(re.sub(r'#[^\n]*', '', m.group(1)))


pla = literal('tools/pla_patches.py', 'PLA_BUILDS')
za = literal('tools/za_patches.py', 'ZA_BUILDS')
la = {m.group(2).lower()[:16]: m.group(1) for m in
      re.finditer(r'GA_VERSION\(\w+,\s*"([^"]+)",\s*"([0-9A-Fa-f]+)"\)', (ROOT / 'runtime/game_addresses/versions.h').read_text())}
violet = {'709bfd6611529864': '4.0.0'}  # (PokeV-Recomp tools/sv_patches.py SV_BUILDS; 4.0.0 since its bb9f414)
assert [r for r in rows if r[3]] == [(k, 'Pok\\xc3\\xa9mon Legends: Arceus', v, True) for k, v in pla.items()], \
    'the one supported build must be tools/pla_patches.py PLA_BUILDS'
want = {**{k: ('Pok\\xc3\\xa9mon Legends: Z-A', None) for k in za},
        **{k: ("The Legend of Zelda: Link's Awakening", v) for k, v in la.items()},
        **{k: ('Pok\\xc3\\xa9mon Violet', v) for k, v in violet.items()}}
foreign = {k: (g, v) for k, g, v, s in rows if not s}
assert set(foreign) == set(want), f'refused builds differ from the sources: {set(foreign) ^ set(want)}'
for k, (game, version) in want.items():
    assert foreign[k][0] == game and (version is None or foreign[k][1] == version), (k, foreign[k], game, version)
print(f'table: {len(rows)} builds, each one from the project\'s sources (PLA_BUILDS, ZA_BUILDS, versions.h, SV_BUILDS)')

# 3. structural: no second table, and the setup goes through the staging transaction (never straight into extracted/)
launcher = (ROOT / 'runtime/ui/launcher.mm').read_text()
main = (ROOT / 'runtime/main.c').read_text()
assert 'return game_identity_supported(id);' in launcher and 'game_identity_supported(id)' in main
for src, name in ((launcher, 'launcher.mm'), (main, 'main.c')):
    for k in table:
        assert ', '.join(f'0x{k[i:i + 2]}' for i in range(0, 8, 2)) not in src, f'{name} lists a build id itself ({k})'
setup = launcher[launcher.index('static void *extract_thread(void *)'):]
setup = setup[:setup.index('\ncheck:')]
assert 'rt_remove_game_files(' not in setup and 'rt_mark_game_files_ours' not in launcher, 'setup must not delete itself'
assert setup.index('rt_game_stage_begin(') < setup.index('import_packages(') < setup.index('stage_commit(stage)')
assert 'remove(lib' not in launcher, '"Recompile game" must delete through rt_remove_game_file'
print('only Pokémon Legends: Arceus 1.1.1 is set up and compiled; Violet / Z-A / Link\'s Awakening are refused by name')
