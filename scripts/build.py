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


def MakefileBuild(conf, arch=None, project=None):
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

    # premake's gmake2 generator emits a target per project alongside
    # "all", so a single project can be built by name. Needed when not
    # every project in the workspace supports the current target OS —
    # e.g. only DXFoliage builds on Linux; DXMaterial/DXTerrain are
    # Windows-only, so "all" would fail there.
    target = project if project else "all"
    subprocess.run((make, f"config={conf.lower()}", target), env=env)

    # Generate compile_commands.json for IDE integration (Zed, clangd, etc.)
    # Parse make's dry-run output to extract clang++ compile commands directly.
    # This avoids depending on compiledb which doesn't recognize clang++.
    # We also inject MSVC/SDK -isystem paths so clangd can find headers
    # without needing the vcvarsall.bat environment.
    try:
        print("Generating compile_commands.json...")
        import json
        import re

        # Build -isystem flags from the INCLUDE environment variable set by
        # vcvarsall.bat so clangd can find Windows SDK and MSVC STL headers.
        system_include_flags = ""
        if env and "INCLUDE" in env:
            for inc_dir in env["INCLUDE"].split(";"):
                inc_dir = inc_dir.strip()
                if inc_dir and os.path.isdir(inc_dir):
                    system_include_flags += f' -isystem "{inc_dir}"'

        # Run dry-run per sub-project so we know each command's working directory.
        # The top-level Makefile calls sub-makes with -C src/{Project}.
        project_root = os.getcwd()
        entries = []

        # Find project subdirectories from the top-level Makefile
        top_makefile = os.path.join(project_root, "Makefile")
        project_dirs = []
        if os.path.isfile(top_makefile):
            with open(top_makefile, "r") as f:
                for mline in f:
                    # Match: @${MAKE} ... -C src/DXMaterial -f Makefile ...
                    m = re.search(r"-C\s+(\S+)\s+-f\s+Makefile", mline)
                    if m:
                        project_dirs.append(m.group(1))

        if not project_dirs:
            # Fallback: run from project root
            project_dirs = ["."]

        for proj_dir in project_dirs:
            abs_proj_dir = os.path.normpath(os.path.join(project_root, proj_dir))
            dry_run = subprocess.run(
                (make, "-Bn", f"config={conf.lower()}"),
                env=env,
                capture_output=True,
                text=True,
                cwd=abs_proj_dir,
            )
            if dry_run.returncode != 0:
                continue

            for line in dry_run.stdout.splitlines():
                # Match clang/clang++/gcc/g++ compile commands with -c flag
                if (
                    re.search(r"\b(clang\+\+|clang|g\+\+|gcc|cc|c\+\+)\b", line)
                    and " -c " in line
                ):
                    # Extract the source file (after -c)
                    src_match = re.search(r'-c\s+"([^"]+)"', line) or re.search(
                        r"-c\s+(\S+)", line
                    )
                    if src_match:
                        src = src_match.group(1)
                        cmd = line.strip()

                        # Fix PCH placeholder path for clangd.
                        # Premake gmake uses -include <objdir>/stdafx.h
                        # where the file is an empty placeholder. Replace
                        # with just the header name so clangd processes the
                        # real header from the source directory.
                        def fix_pch_path(m):
                            path = m.group(2).replace("\\", "/")
                            if "/obj/" in path:
                                return m.group(1) + path.rsplit("/", 1)[-1]
                            return m.group(0)

                        cmd = re.sub(r"(-include\s+)(\S+)", fix_pch_path, cmd)

                        # Inject MSVC/SDK include paths so clangd works
                        # without vcvarsall.bat environment
                        if system_include_flags:
                            cmd += system_include_flags
                        entries.append(
                            {
                                "directory": abs_proj_dir.replace("\\", "/"),
                                "command": cmd,
                                "file": src,
                            }
                        )

        with open("compile_commands.json", "w") as f:
            json.dump(entries, f, indent=2)
        if entries:
            print(f"compile_commands.json generated with {len(entries)} entries.")
        else:
            print(
                "Warning: compile_commands.json generated but no compile commands found."
            )
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
    p.add_argument(
        "project",
        nargs="?",
        default=None,
        help="Build only this project (e.g. DXFoliage). Default: all projects. "
        "Makefile build system only.",
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
        MakefileBuild(args.conf, args.arch, args.project)
    else:
        if args.project:
            print(
                "Note: building a single project is only supported with the "
                "'makefile' build system; building the whole solution."
            )
        MSBuildBuild(args.conf)
