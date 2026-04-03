import sys
import subprocess

subprocess.run(("mox.bat", "clean", "project"))
subprocess.run(("mox.bat", "init", *sys.argv[1:]))
