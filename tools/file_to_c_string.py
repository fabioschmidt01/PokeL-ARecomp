#!/usr/bin/env python3
import sys
import os

def escape_line(line):
    line = line.rstrip('\r\n')
    line = ''.join(c if ord(c) < 128 else '?' for c in line)
    line = line.replace('\\', '\\\\').replace('"', '\\"')
    return f'"{line}\\n"\n'

def convert_smaa(in_path, out_path):
    with open(in_path, 'rb') as f:
        content = f.read().decode('latin1', errors='replace')
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as out:
        for line in content.splitlines():
            out.write(escape_line(line))

def convert_shader_variants(in_path, out_path):
    with open(in_path, 'r', encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as out:
        for line in lines:
            out.write(escape_line(line))
        out.write('""\n')

def convert_fsr2_shaders(out_path, shader_files):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as out:
        for path in shader_files:
            name = os.path.basename(path)
            out.write(f'{{"{name}",\n')
            with open(path, 'rb') as f:
                content = f.read().decode('latin1', errors='replace')
            for line in content.splitlines():
                out.write(escape_line(line))
            out.write('},\n')

if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'smaa':
        convert_smaa(sys.argv[2], sys.argv[3])
    elif mode == 'variants':
        convert_shader_variants(sys.argv[2], sys.argv[3])
    elif mode == 'fsr2':
        out_path = sys.argv[2]
        files = sys.argv[3:]
        convert_fsr2_shaders(out_path, files)
