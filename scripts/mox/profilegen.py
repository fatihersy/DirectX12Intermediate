"""
Generates conan profiles

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
import moxwin
import platform
import shutil
import subprocess

VS_MSVC_MAPPINGS = {
    "17.0": "193",
    "17.1": "193",
    "17.2": "193",
    "17.3": "193",
    "17.4": "193",
    "17.5": "193",
    "17.6": "193",
    "17.7": "193",
    "17.8": "193",
    "17.9": "193",
    "17.10": "194",
    "17.11": "194",
    "17.12": "194",
    "17.13": "194",
    "17.14": "194",
}

VS_YEAR_MSVC_MAPPINGS = {
    "2022": "194",
    "2019": "192",
    "2017": "191",
}

MSVC_MAX_CPPSTD = {
    "194": "23",
    "193": "23",
    "192": "20",
    "191": "17",
}

class INIProfileGen:
    def __init__(self, filename: str, architecture: str, os: str):
        # Open file
        self.file = open(filename, "w", encoding="utf-8")
        # Begin conan profile section
        self.StartSection("settings")
        self.WritePair("arch", architecture)
        self.WritePair("os", os)
        self.WritePair("build_type", "Release")

    def __del__(self):
        # Close file
        if hasattr(self, "file") and not self.file.closed:
            self.file.close()

    def AddGcc(self, cppversion: str, gccversion: str, abiversion: str):
        self.WritePair("compiler", "gcc")
        self.WritePair("compiler.cppstd", cppversion)
        self.WritePair("compiler.version", gccversion)
        self.WritePair("compiler.libcxx", abiversion)

    def AddClang(self, cppversion: str, clangversion: str, abiversion: str):
        self.WritePair("compiler", "clang")
        self.WritePair("compiler.cppstd", cppversion)
        self.WritePair("compiler.version", clangversion)
        # libstdc++11, not libc++: clang on Linux links GCC's libstdc++ by
        # default, and mixing the two standard libraries is an ABI break.
        self.WritePair("compiler.libcxx", abiversion)

    def AddClangCompilerEnv(self):
        """Point dependency builds at clang.

        Conan's compiler *setting* only affects package-id computation; it
        does not choose the compiler. Without CC/CXX, CMake would still
        pick the system default (gcc) and we'd be back to deps built by a
        different compiler than the project.
        """
        self.StartSection("buildenv")
        self.WritePair("CC", "clang")
        self.WritePair("CXX", "clang++")

    def AddMSVC(self, cppversion: str, msvcversion: str, runtime: str):
        self.WritePair("compiler", "msvc")
        self.WritePair("compiler.cppstd", cppversion)
        self.WritePair("compiler.version", msvcversion)
        self.WritePair("compiler.runtime", runtime)

    def AddTempFolder(self, is_windows: bool, tempfolder: str):
        self.StartSection("buildenv")
        if is_windows:
            self.WritePair("TEMP", tempfolder)
            self.WritePair("TMP", tempfolder)
        else:
            self.WritePair("TMPDIR", tempfolder)
            self.WritePair("TEMP", tempfolder)
            self.WritePair("TMP", tempfolder)

    def AddGccCrossLink(self, compilerprefix: str):
        self.StartSection("buildenv")
        self.WritePair("CC", f"{compilerprefix}-gcc")
        self.WritePair("CXX", f"{compilerprefix}-g++")
        self.WritePair("LD", f"{compilerprefix}-ld")

    def AddCMakeModuleWorkaround(self):
        """Let dependencies that ship C++20 modules configure on Linux.

        Modern CMake refuses to configure a target whose sources 'may use'
        C++20 modules unless the generator supports module scanning — only
        Ninja and recent Visual Studio do; 'Unix Makefiles' does not. With
        a new enough GCC this trips on libassert, which fails with:

            The target named "libassert-lib" has C++ sources that may use
            modules, but modules are not supported by this generator

        Prefer Ninja when it's installed (it scans properly and is faster).
        Otherwise fall back to switching scanning off, which is safe here
        because nothing in this project consumes those deps as modules.
        """
        self.StartSection("conf")
        if shutil.which("ninja"):
            self.WritePair("tools.cmake.cmaketoolchain:generator", "Ninja")
        else:
            self.WritePair(
                "tools.cmake.cmaketoolchain:extra_variables",
                "{'CMAKE_CXX_SCAN_FOR_MODULES': 'OFF'}",
            )

    def StartSection(self, section: str):
        # Emit each section header at most once. Several Add* helpers write
        # into the same section (e.g. AddClangCompilerEnv and AddTempFolder
        # both use [buildenv]); duplicate headers happen to be tolerated by
        # INI parsers, but emitting one keeps the profile readable.
        if not hasattr(self, "_sections"):
            self._sections = set()
        if section in self._sections:
            return
        self._sections.add(section)
        self.file.write(f"[{section}]\n")

    def WritePair(self, key: str, value: str):
        self.file.write(f"{key}={value}\n")

def ProfileGen(path: str, architecture: str, cppversion: str, tempfolder: str, vs_year: str = None, compiler: str = None):
    is_windows = platform.system().lower() == "windows"
    platformInfo = mox.GetPlatformInfo(architecture)
    arch = platformInfo["conan_arch"]

    gen = INIProfileGen(path, arch, platform.system())
    if is_windows:
        if vs_year and vs_year in VS_YEAR_MSVC_MAPPINGS:
            msvc_version = VS_YEAR_MSVC_MAPPINGS[vs_year]
        else:
            vs_version = moxwin.FindLatestVisualStudio()[0]["catalog"]["buildVersion"]
            vs_version = ".".join(vs_version.split(".")[:2])
            msvc_version = VS_MSVC_MAPPINGS[vs_version]
        max_std = MSVC_MAX_CPPSTD.get(msvc_version)
        if max_std and int(cppversion) > int(max_std):
            print(f"Warning: MSVC {msvc_version} does not support C++{cppversion}, capping Conan cppstd to {max_std}")
            cppversion = max_std
        gen.AddMSVC(cppversion, msvc_version, "dynamic")
    else:
        # Build dependencies with the same compiler the project itself uses
        # (cmox_compiler in mox.lua). GCC-built deps do link into a
        # clang-built binary on Linux — same Itanium ABI, same libstdc++ —
        # but keeping them consistent means conan's package ids actually
        # describe what was built, and avoids the mismatch biting with LTO
        # or sanitizers later.
        use_clang = compiler is not None and compiler.lower() in ("clang", "clang-cl")
        if use_clang:
            # -dumpversion gives e.g. "21.1.8"; conan settings expect the
            # major version.
            clang_version = subprocess.check_output(("clang++", "-dumpversion"), text=True).strip()
            gen.AddClang(cppversion, clang_version.split(".")[0], "libstdc++11")
        else:
            gcc_version = subprocess.check_output(("g++", "-dumpversion"), text=True).strip()
            gen.AddGcc(cppversion, gcc_version, "libstdc++11")

        if architecture.lower() != platform.machine().lower():
            gen.AddGccCrossLink(platformInfo["gcc_linux_prefix"])
        elif use_clang:
            gen.AddClangCompilerEnv()
    gen.AddTempFolder(is_windows, tempfolder)
    if not is_windows:
        gen.AddCMakeModuleWorkaround()
