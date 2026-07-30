"""
Backs the repository up to every configured destination.

  ./mox.sh push              Push to origin, USB, and cloud
  ./mox.sh push --dry-run    Show what would happen, change nothing
  ./mox.sh push --only usb   Run a single destination (origin|usb|drive)

Each destination is handled independently and a failure in one does not
stop the others. That is the entire reason this exists rather than a git
remote with several push URLs: git walks those in order and aborts at the
first failure, so an unplugged USB drive silently skips the cloud upload.

Cloud backups are uploaded as `git bundle` files - a single immutable file
per push - rather than as a bare repo. A bare repo is thousands of small
files whose refs must stay consistent with their objects, which is exactly
what file-by-file cloud sync cannot promise.
"""

import argparse
import datetime
import os
import shutil
import subprocess
import sys

import moxbackup as mb

DESTINATIONS = ("origin", "usb", "drive")


def PushRefs(target, label):
    """Push every branch and tag to `target`.

    Two calls, not `--all --tags`: git rejects that combination outright
    ("options '--tags' and '--all/--branches' cannot be used together").
    """
    for args in (("--all",), ("--tags",)):
        result = mb.Git("push", target, *args, capture=False)
        if result.returncode != 0:
            mb.Fail(f"push {args[0]} to {label} failed")
            return False
    return True


def PreFlight():
    """Returns True if there is anything worth pushing."""
    mb.Section("Pre-flight")

    if not mb.HasCommits():
        mb.Fail("this repository has no commits yet - run ./mox.sh commit first")
        return False

    dirty = mb.WorkingTreeChanges()
    if dirty:
        print(f"  ! {dirty} uncommitted change(s) - these will NOT be backed up.")
        print("    Only committed work is. Run ./mox.sh commit first if that matters.")
    else:
        mb.Ok("working tree clean")

    mb.Ok(f"HEAD is {mb.GitOut('log', '--oneline', '-1')}")
    return True


def PushOrigin(config, dry_run):
    mb.Section("origin")

    if not config.get("push_origin", True):
        mb.Skip("disabled in config")
        return None
    if not mb.HasRemote("origin"):
        mb.Skip("no 'origin' remote")
        return None

    url = mb.GitOut("remote", "get-url", "origin")
    if dry_run:
        mb.Skip(f"would push all branches and tags to {url}")
        return None

    if not PushRefs("origin", url):
        if url.startswith("https://"):
            # This command never prompts (see GIT_TERMINAL_PROMPT in
            # moxbackup.Git), so credentials have to be established once
            # outside it - plain git, which is allowed to ask.
            print("  ! HTTPS auth failed, and this command cannot prompt for")
            print("    credentials by design. Establish them once with plain git:")
            print("      git push origin HEAD        (password = access token)")
            print("    or switch the remote to SSH:")
            print("      git remote set-url origin git@github.com:USER/REPO.git")
        return False

    mb.Ok(url)
    return True


def PushUsb(config, dry_run):
    mb.Section("usb")

    usb = config.get("usb_path", "")
    if not usb:
        mb.Skip("not configured")
        return None
    usable, note = mb.LocalTargetStatus(usb)
    if not usable:
        mb.Skip(note)
        return None
    if note.startswith("WARNING"):
        print(f"  ! {note}")

    if dry_run:
        action = "create and populate" if not os.path.isdir(usb) else "update"
        mb.Skip(f"would {action} {usb}")
        return None

    if not os.path.isdir(usb):
        init = subprocess.run(("git", "init", "--bare", "-q", usb))
        if init.returncode != 0:
            mb.Fail(f"could not create bare repo at {usb}")
            return False
        mb.Ok(f"created bare repo {usb}")

    # Branches and tags rather than --mirror: mirror deletes refs on the
    # destination that are missing locally, which would propagate local
    # damage into the backup.
    if not PushRefs(usb, usb):
        return False

    mb.Ok(usb)
    return True


def PushDrive(config, dry_run):
    mb.Section("drive")

    drive = config.get("drive_remote", "")
    if not drive:
        mb.Skip("not configured")
        return None

    exe = mb.RcloneExe()
    if not exe:
        mb.Skip("rclone not installed (sudo apt install rclone && rclone config)")
        return None

    remote_name = drive.split(":", 1)[0]
    if remote_name not in mb.RcloneRemotes():
        mb.Skip(f"'{remote_name}' is not a configured rclone remote (rclone config)")
        return None

    stamp = datetime.datetime.now().strftime("%Y-%m-%d-%H%M")
    repo_name = os.path.basename(mb.RepoRoot())
    bundle_name = f"{repo_name}-{stamp}.bundle"

    if dry_run:
        mb.Skip(f"would upload {bundle_name} to {drive}")
        return None

    # A fresh directory per push so the verify step below compares exactly
    # one file rather than every bundle ever made.
    staging = os.path.join(mb.RepoRoot(), "temp", "backup", stamp)
    os.makedirs(staging, exist_ok=True)
    bundle_path = os.path.join(staging, bundle_name)

    if mb.Git("bundle", "create", bundle_path, "--all").returncode != 0:
        mb.Fail("git bundle create failed")
        return False
    mb.Ok(f"bundled {bundle_name} ({os.path.getsize(bundle_path) // 1024} KB)")

    if subprocess.run((exe, "copy", staging, drive)).returncode != 0:
        mb.Fail(f"upload to {drive} failed; bundle kept at {bundle_path}")
        return False

    # rclone compares checksums, which is what catches a truncated upload.
    # `git bundle verify` does NOT - it returns success on a truncated file
    # (tested), so it cannot be the only check here.
    # -q suppresses rclone's NOTICE chatter; errors and the exit code survive.
    if subprocess.run((exe, "check", staging, drive, "--one-way", "-q")).returncode != 0:
        mb.Fail(f"uploaded file does not match local checksum; bundle kept at {bundle_path}")
        return False

    mb.Ok(f"{drive}/{bundle_name} (checksum verified)")
    shutil.rmtree(staging, ignore_errors=True)
    return True


if __name__ == "__main__":
    p = argparse.ArgumentParser(prog="mox push", allow_abbrev=False)
    p.add_argument("--dry-run", action="store_true", help="Show what would happen, change nothing")
    p.add_argument("--only", choices=DESTINATIONS, help="Run a single destination")
    args = p.parse_args()

    config = mb.LoadConfig()
    if not config:
        print(f"No {mb.CONFIG_NAME} found - run ./mox.sh config first.")
        sys.exit(1)

    if not PreFlight():
        sys.exit(1)

    wanted = (args.only,) if args.only else DESTINATIONS
    handlers = {"origin": PushOrigin, "usb": PushUsb, "drive": PushDrive}

    results = {}
    for name in wanted:
        results[name] = handlers[name](config, args.dry_run)

    mb.Section("Summary")

    if args.dry_run:
        print("  dry run - nothing was changed")
        sys.exit(0)

    attempted = [n for n, r in results.items() if r is not None]
    failed = [n for n, r in results.items() if r is False]

    if not attempted:
        print("  nothing was attempted - check ./mox.sh config --show")
        sys.exit(1)

    for name in wanted:
        state = {True: "backed up", False: "FAILED", None: "skipped"}[results[name]]
        print(f"  {name:<8} {state}")

    sys.exit(1 if failed else 0)
