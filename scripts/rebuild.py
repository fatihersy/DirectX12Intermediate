import subprocess

subprocess.run(("mox.bat", "clean", "output"))
subprocess.run(("mox.bat", "build"))
