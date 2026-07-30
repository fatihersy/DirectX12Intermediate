"""
Cleans the build output, then builds again.

  ./mox.sh rebuild              Rebuild everything
  ./mox.sh rebuild DXFoliage    Rebuild one project

Arguments are forwarded to `build` unchanged - it takes the project name
positionally, so there is no --only flag.
"""

import subprocess
import sys

import mox

launcher = mox.Launcher()

# Only the build output: cleaning 'all' here would throw away downloaded
# dependencies and force a full conan re-fetch, which is not what "rebuild"
# should mean.
if subprocess.run((launcher, "clean", "output")).returncode != 0:
    sys.exit(1)

sys.exit(subprocess.run((launcher, "build", *sys.argv[1:])).returncode)
