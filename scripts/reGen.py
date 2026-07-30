"""
Regenerates the project files (clean project, then init).

  ./mox.sh reGen    Regenerate Makefiles / solutions after editing build.lua

Required after adding or removing source files, or editing any build.lua -
`build` reuses the existing Makefile and will silently use stale settings.
"""

import subprocess
import sys

import mox

launcher = mox.Launcher()

if subprocess.run((launcher, "clean", "project")).returncode != 0:
    sys.exit(1)

sys.exit(subprocess.run((launcher, "init", *sys.argv[1:])).returncode)
