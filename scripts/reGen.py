import os
import subprocess
import sys

# The launcher script differs per OS: mox.bat on Windows, mox.sh elsewhere.
# It also has to be addressed by path — the repository root isn't on PATH,
# so a bare name only resolves on Windows, where the current directory is
# searched implicitly.
repoRoot = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mox = os.path.join(repoRoot, "mox.bat" if os.name == "nt" else "mox.sh")

subprocess.run((mox, "clean", "project"))
subprocess.run((mox, "init", *sys.argv[1:]))
