"""
Will build your project
Compiles the project dependent on your system (MSBuild / Makefile)

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
import shutil
import subprocess
import sys

import mox


def FindMake():
    """Find GNU make on the system."""
    # Try 'make' first (works on Linux and Windows if GNU make is on PATH)
    make = shutil.which("make")
    if make:
        return make
    # Try 'mingw32-make' as fallback on Windows
    if not sys.platform.startswith("linux"):
        mingw_make = shutil.which("mingw32-make")
        if mingw_make:
            return mingw_make
    return None


def MSBuildBuild(conf):
    import moxwin

    # Find MSBuild
    vswhere = moxwin.FindLatestVisualStudio()
    vspath = moxwin.GetVisualStudioPath(vswhere)
    msbuild = f"{vspath}\\MSBuild\\Current\\Bin\\MSBuild.exe"

    # Find solution file
    slnFiles = glob.glob("*.sln")
    if len(slnFiles) == 0:
        print("No solution file found! Building not possible!")
        return

    # Run build
    subprocess.run((msbuild, slnFiles[0], f"-p:Configuration={conf}"))


def MakefileBuild(conf, arch=None):
    make = FindMake()
    if make is None:
        print("GNU make not found on PATH! Building not possible!")
        print("Please install GNU make and ensure it is available on your PATH.")
        return

    # On Windows, set up MSVC environment (INCLUDE, LIB, PATH) so the
    # compiler can find Windows SDK headers and libraries
    env = None
    if not sys.platform.startswith("linux"):
        import moxwin

        if arch is None:
            arch = platform.machine().lower()
        try:
            print(f"Setting up MSVC environment via vcvarsall.bat ({arch})...")
            env = moxwin.GetMSVCEnv(arch)
            print("MSVC environment loaded successfully.")

            # When clang targets MSVC ABI (--target=x86_64-pc-windows-msvc,
            # set in libmox.lua), it reads the INCLUDE environment variable
            # directly to find Windows SDK and MSVC STL headers. The INCLUDE
            # var is already set by vcvarsall.bat and passed through in env.
            # DirectX headers are provided via vcpkg -isystem paths in the
            # Makefile, which take priority over system headers.

        except (FileNotFoundError, RuntimeError) as e:
            print(f"Warning: Could not load MSVC environment: {e}")
            print(
                "Building without MSVC environment. Windows SDK headers may not be found."
            )

    subprocess.run((make, f"config={conf.lower()}", "all"), env=env)

    # Generate compile_commands.json for IDE integration (Zed, clangd, etc.)
    try:
        print("Generating compile_commands.json...")
        result = subprocess.run(
            (sys.executable, "-m", "compiledb", "-n", "-o", "compile_commands.json",
             make, f"config={conf.lower()}", "all"),
            env=env,
            capture_output=True,
        )
        if result.returncode == 0 and os.path.isfile("compile_commands.json"):
            print("compile_commands.json generated in project root.")
        else:
            print("Warning: Could not generate compile_commands.json.")
    except Exception as e:
        print(f"Warning: compile_commands.json generation skipped: {e}")


if __name__ == "__main__":
    # Configuration from cli
    p = argparse.ArgumentParser(prog="build.py", allow_abbrev=False)
    p.add_argument(
        "--conf", default="Debug", help="Build configuration (default: Debug)"
    )
    p.add_argument(
        "--build-system",
        default=None,
        help="Build system: 'visualstudio' or 'makefile'. Overrides mox.lua config.",
    )
    p.add_argument(
        "--arch",
        default=None,
        help="Architecture for MSVC environment setup (e.g. x64, x86, arm64)",
    )
    args = p.parse_args()

    # Resolve build system
    buildSystem = args.build_system
    if buildSystem is None:
        buildSystem = mox.ExtractLuaDef("./mox.lua", "cmox_build_system")
        if buildSystem is None:
            buildSystem = "visualstudio"

    # Run build step
    if buildSystem == "makefile":
        MakefileBuild(args.conf, args.arch)
    else:
        MSBuildBuild(args.conf)
