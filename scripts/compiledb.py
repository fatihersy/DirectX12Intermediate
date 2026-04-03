"""
Generate compile_commands.json from Makefile dry-run
Places the file in the project root for IDE integration (Zed, clangd, etc.)

Requires:
- Makefile builds (cmox_build_system = "makefile" in mox.lua)
- compiledb (installed via requirements.txt)

Copyright (c) 2025 Moxibyte GmbH

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
import mox

import os
import sys
import shutil
import platform
import argparse
import subprocess

def FindMake():
    """Find GNU make on the system."""
    make = shutil.which('make')
    if make:
        return make
    if not sys.platform.startswith('linux'):
        mingw_make = shutil.which('mingw32-make')
        if mingw_make:
            return mingw_make
    return None

if __name__ == '__main__':
    p = argparse.ArgumentParser(prog="compiledb.py", allow_abbrev=False)
    p.add_argument("--conf", default="Release", help="Build configuration (default: Release)")
    args = p.parse_args()

    # Verify build system is makefile
    buildSystem = mox.ExtractLuaDef("./mox.lua", "cmox_build_system")
    if buildSystem != "makefile":
        print("Error: compile_commands.json generation requires cmox_build_system = \"makefile\"")
        print(f"Current build system: {buildSystem}")
        print("Run 'mox init --build-system makefile' first, then try again.")
        sys.exit(1)

    make = FindMake()
    if make is None:
        print('GNU make not found on PATH!')
        sys.exit(1)

    # On Windows, load MSVC environment for correct include paths
    env = None
    if not sys.platform.startswith('linux'):
        import moxwin
        arch = platform.machine().lower()
        try:
            print(f'Loading MSVC environment ({arch})...')
            env = moxwin.GetMSVCEnv(arch)
        except (FileNotFoundError, RuntimeError) as e:
            print(f'Warning: Could not load MSVC environment: {e}')

    # Run make dry-run and pipe to compiledb
    print(f'Generating compile_commands.json (config={args.conf.lower()})...')
    result = subprocess.run(
        (sys.executable, '-m', 'compiledb', '-n', '-o', 'compile_commands.json',
         'make', f'config={args.conf.lower()}', 'all'),
        env=env
    )

    if result.returncode == 0 and os.path.isfile('compile_commands.json'):
        print('compile_commands.json generated in project root.')
    else:
        print('Failed to generate compile_commands.json.')
        sys.exit(1)
