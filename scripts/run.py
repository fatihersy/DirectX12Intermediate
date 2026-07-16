"""
Project execution script
This script will run the compiled application in the proper way.

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

import argparse
import glob
import os
import platform
import subprocess
import sys

import mox

if __name__ == "__main__":
    p = argparse.ArgumentParser(prog="run.py", allow_abbrev=False)
    p.add_argument(
        "--conf", default="Debug", help="Build configuration (default: Debug)"
    )
    p.add_argument(
        "--arch",
        default=platform.machine().lower(),
        help="Alternative (cross compile) architecture",
    )
    p.add_argument(
        "--proj", nargs="?", help="Executable/project name in build/{arch}-{conf}/bin"
    )
    args, passthrough = p.parse_known_args()

    conf = args.conf
    arch = mox.GetPlatformInfo(args.arch)["premake_arch"]

    if args.proj:
        proj = args.proj
        bindir = f"build/{arch}-{conf}/bin/{proj}"

        # The argument can be a direct executable path or a project name.
        # For project names, the exe lives inside a subdirectory:
        #   build/{arch}-{conf}/bin/{ProjectName}/{output_name}.exe
        # Try the path directly first; if it's a directory, find the exe inside.
        if os.path.isdir(bindir):
            exes = (
                glob.glob(os.path.join(bindir, "*.exe"))
                if sys.platform == "win32"
                else [
                    f
                    for f in glob.glob(os.path.join(bindir, "*"))
                    if os.access(f, os.X_OK) and os.path.isfile(f)
                ]
            )
            if len(exes) == 1:
                exepath = exes[0]
            elif len(exes) > 1:
                print(f"Multiple executables found in {bindir}:")
                for e in exes:
                    print(f"  {os.path.basename(e)}")
                sys.exit(1)
            else:
                print(f"No executable found in {bindir}")
                sys.exit(1)
        else:
            exepath = bindir

        if sys.platform.startswith("linux"):
            exepath = "../" + exepath
        else:
            exepath = "./" + exepath

        path = f"./app/{proj}"
        os.makedirs(path, exist_ok=True)
        proc = subprocess.Popen((exepath, *passthrough), cwd=path)

        try:
            returncode = proc.wait()
        except KeyboardInterrupt:
            print("\nInterrupted...")
            proc.terminate()
            returncode = proc.wait()
            sys.exit(130)

        sys.exit(returncode)
    else:
        print("No executable provided!")
