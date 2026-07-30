"""
Cleans everything, then re-initialises from scratch.

  ./mox.sh reinit    Full reset: build output, projects, dependencies, vcpkg

Arguments are forwarded to `init` unchanged.
"""

import subprocess
import sys

import mox

launcher = mox.Launcher()

if subprocess.run((launcher, "clean", "all")).returncode != 0:
    sys.exit(1)

sys.exit(subprocess.run((launcher, "init", *sys.argv[1:])).returncode)
