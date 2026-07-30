"""
Shared helpers for the backup commands (config / commit / push).

Not a mox command itself - `mox.py` lists every .py in this directory as a
runnable script, but this one only ever gets imported.
"""

import json
import os
import shutil
import subprocess
import sys

CONFIG_NAME = "mox.backup.json"


def RepoRoot():
    """Repository root, resolved from this file rather than the cwd.

    mox.sh invokes everything with $(pwd) baked in, so a script that trusts
    the working directory breaks the moment it is run from a subdirectory.
    """
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def ConfigPath():
    return os.path.join(RepoRoot(), CONFIG_NAME)


def LoadConfig():
    """Returns the saved settings, or an empty dict if never configured."""
    path = ConfigPath()
    if not os.path.isfile(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"! {CONFIG_NAME} is unreadable ({e}); treating as unconfigured")
        return {}


def SaveConfig(config):
    with open(ConfigPath(), "w", encoding="utf-8") as f:
        json.dump(config, f, indent=4)
        f.write("\n")


def Git(*args, capture=True, check=False):
    """Run git in the repository root. Returns a CompletedProcess.

    GIT_TERMINAL_PROMPT=0 makes git fail immediately instead of asking for
    a username/password on the terminal. A backup command that stops to ask
    a question is a backup command that hangs forever when it is run
    unattended, and HTTPS credential prompts are the usual cause.
    """
    env = dict(os.environ, GIT_TERMINAL_PROMPT="0")
    return subprocess.run(
        ("git", "-C", RepoRoot(), *args),
        capture_output=capture,
        text=True,
        check=check,
        env=env,
    )


def GitOut(*args):
    """Run git and return stripped stdout, or '' on failure."""
    result = Git(*args)
    return result.stdout.strip() if result.returncode == 0 else ""


def HasRemote(name):
    return name in GitOut("remote").split()


def CurrentBranch():
    return GitOut("rev-parse", "--abbrev-ref", "HEAD")


def HasCommits():
    return Git("rev-parse", "--verify", "HEAD").returncode == 0


def WorkingTreeChanges():
    """Number of changed/untracked entries. 0 means the tree is clean."""
    out = GitOut("status", "--porcelain")
    return len(out.splitlines()) if out else 0


def GitIdentity():
    """(name, email) from git config, either may be '' if unset."""
    return GitOut("config", "user.name"), GitOut("config", "user.email")


def RequireIdentity():
    """True if git knows who you are; otherwise explains and returns False."""
    name, email = GitIdentity()
    if name and email:
        return True

    print("! Git identity is not set - commits would be rejected.")
    print("  Run:  ./mox.sh config")
    return False


def RcloneExe():
    return shutil.which("rclone")


def RcloneRemotes():
    """Configured rclone remote names, without the trailing colon."""
    exe = RcloneExe()
    if not exe:
        return []
    result = subprocess.run((exe, "listremotes"), capture_output=True, text=True)
    if result.returncode != 0:
        return []
    return [line.strip().rstrip(":") for line in result.stdout.splitlines() if line.strip()]


def MountPointOf(path):
    """The mount point the given path lives on (or would live on)."""
    p = os.path.abspath(path)

    # The bare repo usually does not exist yet on the first push, so walk
    # up to the nearest directory that does.
    while not os.path.exists(p):
        parent = os.path.dirname(p)
        if parent == p:
            break
        p = parent

    while not os.path.ismount(p):
        parent = os.path.dirname(p)
        if parent == p:
            break
        p = parent

    return p


def LocalTargetStatus(path):
    """Can we back up to `path` right now? Returns (usable, note).

    Two distinct situations, and conflating them is how you end up with a
    backup that silently isn't one:

      - the containing directory is missing  -> drive unplugged, skip
      - it exists but sits on the root disk  -> usable, but it is NOT a
        separate physical device, so say so out loud
    """
    if not path:
        return False, "not configured"

    parent = os.path.dirname(os.path.abspath(path).rstrip(os.sep)) or os.sep
    if not os.path.isdir(parent):
        return False, f"drive not mounted ({parent} does not exist)"

    if MountPointOf(path) == os.sep:
        return True, "WARNING: on the root filesystem, not a separate drive"

    return True, f"mounted at {MountPointOf(path)}"


# All reporting goes to stdout with flush=True. Subprocesses (git, rclone)
# write straight to the terminal, so an unflushed buffer here makes their
# output appear before the section header it belongs under.
def Section(title):
    print(f"\n== {title}", flush=True)


def Ok(message):
    print(f"  [ok]   {message}", flush=True)


def Skip(message):
    print(f"  [skip] {message}", flush=True)


def Fail(message):
    print(f"  [FAIL] {message}", flush=True)
