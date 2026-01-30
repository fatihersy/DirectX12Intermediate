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

def GetPremakeGenerator():
    if sys.platform.startswith('linux'):
        return 'gmake'
    else:
        vswhere = moxwin.FindLatestVisualStudio()
        vsversion = moxwin.GetVisualStudioYearNumber(vswhere)
        return f'vs{vsversion}'

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

def InitializeVcpkg():
    """Download vcpkg and install dependencies"""
    vcpkgPath = os.path.abspath('./dependencies/vcpkg')
    vcpkgInstalledPath = os.path.abspath('./dependencies/vcpkg_installed')
    vcpkgExe = os.path.join(vcpkgPath, GetExecutable('vcpkg'))
    
    # Download and bootstrap vcpkg if needed
    DownloadVcpkg()
    
    # Install vcpkg dependencies from vcpkg.json
    if os.path.exists('vcpkg.json'):
        print('Installing vcpkg dependencies...')
        triplet = 'x64-windows' if sys.platform.startswith('win') else 'x64-linux'
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
    args = p.parse_args()

    skipConan = args.skip_conan
    skipVcpkg = args.skip_vcpkg
    arch = args.arch
    conanReleaseOnly = args.conan_release_only

    # Create temp folder
    tempFolder = str(os.path.abspath("./dependencies/conan-temp"))
    os.makedirs(tempFolder, exist_ok=True)

    # Generate conan profiles
    os.makedirs("./profiles/", exist_ok=True)
    cpp_version = re.search(r'(\d+)', mox.ExtractLuaDef("./mox.lua", "cmox_cpp_version")).group(1)
    mox.ProfileGen("./profiles/build", platform.machine().lower(), cpp_version, tempFolder)
    mox.ProfileGen(f"./profiles/host_{arch}", arch, cpp_version, tempFolder)

    # Download tool applications
    DownloadPremake()

    # Initialize vcpkg
    if not skipVcpkg:
        print('\n=== Initializing vcpkg ===')
        InitializeVcpkg()

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

    # GCC Prefix
    gccPrefix = hostArch[f'gcc_{ "linux" if sys.platform.startswith("linux") else "windows"  }_prefix'] + '-'

    # Get vcpkg installed path for premake
    vcpkgInstalledRoot = os.path.abspath('./dependencies/vcpkg_installed')

    # Run premake5
    premakeGenerator = GetPremakeGenerator()
    subprocess.run((
        './dependencies/premake5/premake5',
        f'--mox_conan_arch={ hostArch["conan_arch"] }',
        f'--mox_premake_arch={ hostArch["premake_arch"] }',
        f'--mox_gcc_prefix={ gccPrefix }',
        f'--mox_version={ version }',
        f'--mox_conan_release_only={ conanReleaseOnly }',
        f'--mox_vcpkg_root={ vcpkgInstalledRoot }',
        '--file=./scripts/premake5.lua',
        premakeGenerator
    ))
