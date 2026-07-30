"""
Project initialization script
This will initialize your VisualStudio solution / Your makefile

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

import glob
import os
import re
import sys
import stat
import zipfile
import tarfile
import platform
import argparse
import subprocess
import urllib.request

DEFAULT_TO_CONAN_ALWAY_RELEASE = True

def GetExecutable(exe):
    if sys.platform.startswith('linux'):
        return exe
    else:
        return f'{exe}.exe'

VS_TOOLSET_MAP = {
    '2022': 'v143',
    '2019': 'v142',
    '2017': 'v141',
}

def GetPremakeGenerator(build_system, vs_version=None, compiler=None):
    if build_system == 'makefile':
        if compiler is None:
            if sys.platform.startswith('linux'):
                compiler = 'gcc'
            else:
                compiler = 'msvc'
        return 'gmake', compiler
    else:
        if vs_version:
            toolset = VS_TOOLSET_MAP.get(str(vs_version), '')
        else:
            vswhere = moxwin.FindLatestVisualStudio()
            vs_version = moxwin.GetVisualStudioYearNumber(vswhere)
            toolset = VS_TOOLSET_MAP.get(vs_version, '')
        # Override toolset for clang-cl (LLVM/Clang with MSVC compatibility)
        if compiler == 'clang-cl':
            toolset = 'ClangCL'
        return f'vs{vs_version}', toolset

def GetPremakeDownloadUrl(version):
    baseUrl = f'https://github.com/premake/premake-core/releases/download/v{version}/premake-{version}'
    if sys.platform.startswith('linux'):
        return baseUrl + '-linux.tar.gz'
    else:
        return baseUrl + '-windows.zip'

def DownloadPremake(version = '5.0.0-beta8'):
    premakeDownloadUrl = GetPremakeDownloadUrl(version)
    premakeTargetFolder = './dependencies/premake5'
    premakeTargetZip = f'{premakeTargetFolder}/premake5.tmp'
    premakeTargetExe = f'{premakeTargetFolder}/{GetExecutable("premake5")}'

    if not os.path.exists(premakeTargetExe):
        print('Downloading premake5...')
        os.makedirs(premakeTargetFolder, exist_ok=True)
        urllib.request.urlretrieve(premakeDownloadUrl, premakeTargetZip)

        if premakeDownloadUrl.endswith('zip'):
            with zipfile.ZipFile(premakeTargetZip, 'r') as zipFile:
                zipFile.extract('premake5.exe', premakeTargetFolder)
        else:
            with tarfile.open(premakeTargetZip, 'r') as tarFile:
                tarFile.extractall(premakeTargetFolder, filter=tarfile.data_filter)
            os.chmod(premakeTargetExe, os.stat(premakeTargetExe).st_mode | stat.S_IEXEC)


def VersionKey(path):
    """Sort key that orders 1.45 above 1.9, which string sorting does not."""
    match = re.search(r'/wayland-protocols/([^/]+)/res/', path)
    if not match:
        return ()
    return tuple(int(part) if part.isdigit() else 0 for part in match.group(1).split('.'))


def GenerateWaylandProtocols(targetOs):
    """Turn the Wayland protocol XML into C bindings.

    Wayland ships its window-management protocol (xdg-shell) as an XML
    interface description rather than a library: wayland-scanner reads it
    and emits the client-side marshalling code. It's the moral equivalent
    of windows.h declaring CreateWindow — a fixed API surface, not build
    output — so it's generated once here rather than on every build. The
    only thing that changes it is the pinned wayland-protocols version in
    conanfile.py, and changing that means re-running init anyway.

    Both inputs come from conan (see conanfile.py), so this must run after
    'conan install'.

    Protocols are listed by their full path inside the wayland-protocols
    tree, because that tree is not flat: a protocol lives under stable/,
    unstable/, or staging/ depending on maturity, and the file name does
    not follow from the directory name (viewporter/viewporter.xml but
    xdg-decoration/xdg-decoration-unstable-v1.xml). Spelling the path out
    is unambiguous - 'xdg-shell' alone would match both the stable
    protocol and two obsolete unstable revisions - and documents which
    stability tier we depend on.
    """
    if targetOs != 'linux':
        return

    outDir = './dependencies/wayland'
    protocols = [
        # Window management: title, close, resize, fullscreen.
        'stable/xdg-shell/xdg-shell.xml',
        # HiDPI. fractional-scale reports a non-integer scale; viewporter
        # is how a buffer at that scale declares its logical size, since
        # wl_surface.set_buffer_scale only accepts integers.
        'staging/fractional-scale/fractional-scale-v1.xml',
        'stable/viewporter/viewporter.xml',
        # Cursor by semantic name (default / text / *-resize), so hiding
        # and restoring the pointer needs no cursor theme, no wl_buffer,
        # and no animation timer. Wayland core has no "restore default
        # cursor" request, which is what makes this worth a dependency.
        'staging/cursor-shape/cursor-shape-v1.xml',
        # Not used directly. cursor-shape's get_tablet_tool_v2 request
        # takes a zwp_tablet_tool_v2, so its generated code references
        # that interface symbol and will not link without this.
        'unstable/tablet/tablet-unstable-v2.xml',
    ]

    scanners = glob.glob('./dependencies/full_deploy/build/wayland/*/*/*/bin/wayland-scanner')
    if not scanners:
        print('Warning: wayland-scanner not found in conan output; skipping protocol generation')
        return
    scanner = scanners[0]

    os.makedirs(outDir, exist_ok=True)
    for relPath in protocols:
        matches = glob.glob(f'./dependencies/full_deploy/host/wayland-protocols/*/res/wayland-protocols/{relPath}')
        if not matches:
            print(f'Warning: {relPath} not found in conan output; skipping')
            continue

        # conan's full_deploy ACCUMULATES: bumping a version leaves the old
        # tree in place beside the new one. Taking matches[0] then silently
        # picks whichever sorts first - which is how a bump to
        # wayland-protocols 1.45 kept generating from 1.31. Sort by parsed
        # version so the newest always wins, and say so when there is more
        # than one, because a stale deploy is worth knowing about.
        if len(matches) > 1:
            matches.sort(key=lambda m: VersionKey(m))
            print(f'Note: {len(matches)} deployed versions of wayland-protocols; using the newest')
        xml = matches[-1] if len(matches) > 1 else matches[0]

        # Output names follow the XML file, not the directory, which is
        # the convention every Wayland client uses for the generated
        # headers (fractional-scale-v1-client-protocol.h).
        name = os.path.splitext(os.path.basename(xml))[0]

        # private-code: the marshalling implementation, compiled into us.
        # client-header: the declarations our C++ includes.
        for mode, ext in (('private-code', 'protocol.c'), ('client-header', 'client-protocol.h')):
            dest = f'{outDir}/{name}-{ext}'
            subprocess.run((scanner, mode, xml, dest), check=True)
            print(f'Generated {dest}')


def GetVcpkgDownloadUrl():
    """Get the download URL for vcpkg based on the platform"""
    # vcpkg is distributed as source, we clone/download and bootstrap
    return 'https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip'

def DownloadVcpkg():
    """Download and bootstrap vcpkg"""
    vcpkgTargetFolder = './dependencies/vcpkg'
    vcpkgTargetZip = './dependencies/vcpkg.tmp.zip'
    vcpkgExe = f'{vcpkgTargetFolder}/{GetExecutable("vcpkg")}'
    
    if not os.path.exists(vcpkgExe):
        print('Downloading vcpkg...')
        os.makedirs('./dependencies', exist_ok=True)
        
        # Download vcpkg
        urllib.request.urlretrieve(GetVcpkgDownloadUrl(), vcpkgTargetZip)
        
        # Extract vcpkg
        with zipfile.ZipFile(vcpkgTargetZip, 'r') as zipFile:
            # vcpkg archive extracts to vcpkg-master/, we need to rename it
            zipFile.extractall('./dependencies')
        
        # Rename extracted folder
        extractedFolder = './dependencies/vcpkg-master'
        if os.path.exists(extractedFolder):
            if os.path.exists(vcpkgTargetFolder):
                import shutil
                shutil.rmtree(vcpkgTargetFolder)
            os.rename(extractedFolder, vcpkgTargetFolder)
        
        # Clean up zip file
        os.remove(vcpkgTargetZip)
        
        # Bootstrap vcpkg
        print('Bootstrapping vcpkg...')
        if sys.platform.startswith('win'):
            bootstrapScript = os.path.join(os.path.abspath(vcpkgTargetFolder), 'bootstrap-vcpkg.bat')
            subprocess.run([bootstrapScript], cwd=os.path.abspath(vcpkgTargetFolder), check=True)
        else:
            bootstrapScript = os.path.join(os.path.abspath(vcpkgTargetFolder), 'bootstrap-vcpkg.sh')
            subprocess.run(['bash', bootstrapScript], cwd=os.path.abspath(vcpkgTargetFolder), check=True)
        
        print('vcpkg downloaded and bootstrapped successfully.')
    else:
        print('vcpkg already available.')

VCPKG_ARCH_MAP = {
    'x86':      'x86',
    'i386':     'x86',
    'i686':     'x86',
    'x86_64':   'x64',
    'amd64':    'x64',
    'arm':      'arm',
    'armhf':    'arm',
    'armv7l':   'arm',
    'armv8l':   'arm',
    'arm64':    'arm64',
    'aarch64':  'arm64',
}

def GetVcpkgTriplet(arch, target_os):
    """Derive vcpkg triplet from target architecture and OS."""
    vcpkg_arch = VCPKG_ARCH_MAP.get(arch.lower(), 'x64')
    return f'{vcpkg_arch}-{target_os}'

def InitializeVcpkg(arch, target_os):
    """Download vcpkg and install dependencies"""
    vcpkgPath = os.path.abspath('./dependencies/vcpkg')
    vcpkgInstalledPath = os.path.abspath('./dependencies/vcpkg_installed')
    vcpkgExe = os.path.join(vcpkgPath, GetExecutable('vcpkg'))

    # Download and bootstrap vcpkg if needed
    DownloadVcpkg()

    # Install vcpkg dependencies from vcpkg.json
    if os.path.exists('vcpkg.json'):
        print('Installing vcpkg dependencies...')
        triplet = GetVcpkgTriplet(arch, target_os)
        print(f'Using vcpkg triplet: {triplet}')
        subprocess.run([
            vcpkgExe,
            'install',
            f'--x-install-root={vcpkgInstalledPath}',
            f'--vcpkg-root={vcpkgPath}',
            f'--triplet={triplet}'
        ], check=True)
        print('vcpkg dependencies installed successfully.')
    else:
        print('No vcpkg.json found. Skipping vcpkg package installation.')

    return True

def ConanBuild(conf, host_profile, build_profile):
    return (
        'conan', 'install', '.',
        '--build', 'missing',
        f'--profile:host=./profiles/{host_profile}',
        f'--profile:build=./profiles/{build_profile}',
        f'--output-folder=./dependencies',
        f'--deployer=full_deploy',
        f'--settings=build_type={conf}'
    )

if __name__ == '__main__':
    # Cli
    p = argparse.ArgumentParser(prog="init.py", allow_abbrev=False)
    p.add_argument("--skip-conan", action="store_true", help="Skip Conan evaluation")
    p.add_argument("--skip-vcpkg", action="store_true", help="Skip vcpkg initialization")
    p.add_argument("--arch", default=platform.machine().lower(), help="Alternative (cross compile) architecture")
    p.add_argument("--conan-release-only", action=argparse.BooleanOptionalAction, default=DEFAULT_TO_CONAN_ALWAY_RELEASE, help="Forces conan into only generating release dependencies.")
    p.add_argument("--vs-version", default=None, help="Visual Studio version year (e.g. 2022, 2019). Defaults to latest installed.")
    p.add_argument("--build-system", default=None, help="Build system: 'visualstudio' or 'makefile'. Overrides mox.lua config.")
    p.add_argument("--compiler", default=None, help="Compiler: 'msvc', 'gcc', 'clang', 'clang-cl'. Overrides mox.lua config.")
    p.add_argument("--target-os", default=None, help="Target OS: 'windows' or 'linux'. Defaults to host OS.")
    args = p.parse_args()

    skipConan = args.skip_conan
    skipVcpkg = args.skip_vcpkg
    arch = args.arch
    conanReleaseOnly = args.conan_release_only
    buildSystem = args.build_system
    compiler = args.compiler

    # Resolve target OS (CLI > host platform)
    targetOs = args.target_os
    if targetOs is None:
        targetOs = "linux" if sys.platform.startswith("linux") else "windows"

    # Create temp folder
    tempFolder = str(os.path.abspath("./dependencies/conan-temp"))
    os.makedirs(tempFolder, exist_ok=True)

    # Resolve Visual Studio version
    vs_version = args.vs_version or mox.ExtractLuaDef("./mox.lua", "cmox_vs_version")

    # Resolve build system (CLI > config)
    if buildSystem is None:
        buildSystem = mox.ExtractLuaDef("./mox.lua", "cmox_build_system")
        if buildSystem is None:
            buildSystem = "visualstudio"

    # Resolve compiler (CLI > config)
    if compiler is None:
        compiler = mox.ExtractLuaDef("./mox.lua", "cmox_compiler")

    # Generate conan profiles
    os.makedirs("./profiles/", exist_ok=True)
    cpp_version = re.search(r'(\d+)', mox.ExtractLuaDef("./mox.lua", "cmox_cpp_version")).group(1)
    mox.ProfileGen("./profiles/build", platform.machine().lower(), cpp_version, tempFolder, vs_version, compiler)
    mox.ProfileGen(f"./profiles/host_{arch}", arch, cpp_version, tempFolder, vs_version, compiler)

    # Download tool applications
    DownloadPremake()

    # Initialize vcpkg
    if not skipVcpkg:
        print('\n=== Initializing vcpkg ===')
        InitializeVcpkg(arch, targetOs)

    # Get system architecture
    buildArch = mox.GetThisPlatformInfo()
    hostArch = mox.GetPlatformInfo(arch)
    print(f'Generating project on { platform.machine().lower() } (conan={ buildArch["conan_arch"] } and premake={buildArch["premake_arch"]})')
    print(f'for {arch} (conan={ hostArch["conan_arch"] } and premake={hostArch["premake_arch"]})')

    # Version detection
    version = mox.GetAppVersion()
    print(f'Version is { version }')

    # Generate conan project
    if not skipConan:
        if not conanReleaseOnly:
            subprocess.run(ConanBuild('Debug', f'host_{arch}', 'build'))
        subprocess.run(ConanBuild('Release', f'host_{arch}', 'build'))
        # Copy conan dlls
        subprocess.run((
            sys.executable,
            './scripts/copydlls.py',
            arch
        ))

    # Generate Wayland protocol bindings (needs conan's wayland-scanner +
    # wayland-protocols, so it has to come after the conan step)
    GenerateWaylandProtocols(targetOs)

    # GCC Prefix (based on target OS, not host)
    gccPrefix = hostArch[f'gcc_{targetOs}_prefix'] + '-'

    # Get vcpkg installed path for premake
    vcpkgInstalledRoot = os.path.abspath('./dependencies/vcpkg_installed')

    # Run premake5
    premakeGenerator, generatorExtra = GetPremakeGenerator(buildSystem, vs_version, compiler)
    premakeArgs = [
        './dependencies/premake5/premake5',
        f'--mox_conan_arch={ hostArch["conan_arch"] }',
        f'--mox_premake_arch={ hostArch["premake_arch"] }',
        f'--mox_gcc_prefix={ gccPrefix }',
        f'--mox_version={ version }',
        f'--mox_conan_release_only={ conanReleaseOnly }',
        f'--mox_vcpkg_root={ vcpkgInstalledRoot }',
        f'--mox_target_os={ targetOs }',
        '--file=./scripts/premake5.lua',
    ]
    # Only pass --mox_vs_toolset for Visual Studio builds
    if buildSystem == 'visualstudio' and generatorExtra:
        premakeArgs.append(f'--mox_vs_toolset={ generatorExtra }')
    # Only pass --mox_compiler when it has a value
    if generatorExtra:
        premakeArgs.append(f'--mox_compiler={ generatorExtra }')
    premakeArgs.append(premakeGenerator)
    subprocess.run(premakeArgs)
