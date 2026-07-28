"""
Sets up the identity and backup destinations the other commands need.

  ./mox.sh config           Interactive setup
  ./mox.sh config --show    Print current settings and exit

Git identity is written to your GLOBAL git config, because that is where
git itself reads it from. The backup destinations are written to
mox.backup.json in the repository root, which is gitignored - they are
properties of this machine, not of the project.
"""

import argparse
import os
import subprocess
import sys

import moxbackup as mb


def Prompt(label, current, allow_empty=True):
    """Ask for a value, offering the current one as the default."""
    suffix = f" [{current}]" if current else ""
    answer = input(f"{label}{suffix}: ").strip()

    if not answer:
        if current:
            return current
        if allow_empty:
            return ""
        print("  (required)")
        return Prompt(label, current, allow_empty)
    return answer


def Show(config):
    name, email = mb.GitIdentity()

    print("Git identity (global)")
    print(f"  user.name   {name or '-- not set'}")
    print(f"  user.email  {email or '-- not set'}")

    print()
    print(f"Backup destinations ({mb.CONFIG_NAME})")
    if not config:
        print("  -- not configured; run ./mox.sh config")
        return

    usb = config.get("usb_path", "")
    drive = config.get("drive_remote", "")

    if usb:
        _, note = mb.LocalTargetStatus(usb)
        print(f"  usb_path      {usb}  <- {note}")
    else:
        print("  usb_path      -- not set")
    print(f"  drive_remote  {drive or '-- not set'}")
    print(f"  push_origin   {config.get('push_origin', True)}")


def ConfigureIdentity():
    name, email = mb.GitIdentity()

    print("Git identity - this is what gets stamped on every commit.")
    new_name = Prompt("  Name", name, allow_empty=False)
    new_email = Prompt("  Email", email, allow_empty=False)

    if new_name != name:
        subprocess.run(("git", "config", "--global", "user.name", new_name), check=True)
    if new_email != email:
        subprocess.run(("git", "config", "--global", "user.email", new_email), check=True)

    mb.Ok(f"{new_name} <{new_email}>")


def ConfigureUsb(config):
    print()
    print("Local backup - path to a bare repo on your USB drive.")
    print("It is created on first push; the drive just has to be mounted.")
    print("Leave empty to skip local backups.")

    usb = Prompt("  USB repo path", config.get("usb_path", ""))
    config["usb_path"] = usb

    if not usb:
        mb.Skip("no local backup configured")
        return

    usable, note = mb.LocalTargetStatus(usb)
    if usable:
        mb.Ok(f"{usb} ({note})")
    else:
        mb.Skip(f"{usb} ({note}) - that is fine, push will skip it")


def ConfigureDrive(config):
    print()
    print("Cloud backup - an rclone remote, as 'remote:path'.")
    print("Bundles are uploaded there, one dated file per push.")
    print("Leave empty to skip cloud backups.")

    exe = mb.RcloneExe()
    if not exe:
        print("  ! rclone is not installed. To use this:")
        print("      sudo apt install rclone && rclone config")
    else:
        remotes = mb.RcloneRemotes()
        if remotes:
            print(f"  Configured rclone remotes: {', '.join(remotes)}")
        else:
            print("  ! rclone is installed but has no remotes yet. Run: rclone config")

    drive = Prompt("  Drive remote", config.get("drive_remote", ""))
    config["drive_remote"] = drive

    if not drive:
        mb.Skip("no cloud backup configured")
        return

    remote_name = drive.split(":", 1)[0]
    if exe and remote_name not in mb.RcloneRemotes():
        mb.Skip(f"'{remote_name}' is not a configured rclone remote yet - run: rclone config")
    else:
        mb.Ok(drive)


def ConfigureOrigin(config):
    print()
    if not mb.HasRemote("origin"):
        config["push_origin"] = False
        mb.Skip("no 'origin' remote in this repository")
        return

    url = mb.GitOut("remote", "get-url", "origin")
    print(f"Also push to 'origin' ({url}) on every backup?")
    answer = input("  [Y/n]: ").strip().lower()
    config["push_origin"] = answer not in ("n", "no")
    mb.Ok("origin included" if config["push_origin"] else "origin excluded")


if __name__ == "__main__":
    p = argparse.ArgumentParser(prog="mox config", allow_abbrev=False)
    p.add_argument("--show", action="store_true", help="Print current settings and exit")
    args = p.parse_args()

    config = mb.LoadConfig()

    if args.show:
        Show(config)
        sys.exit(0)

    try:
        ConfigureIdentity()
        ConfigureUsb(config)
        ConfigureDrive(config)
        ConfigureOrigin(config)
    except KeyboardInterrupt:
        print()
        print("Cancelled; nothing was saved.")
        sys.exit(130)

    mb.SaveConfig(config)

    print()
    print(f"Saved to {mb.CONFIG_NAME}")
    print("Next:  ./mox.sh commit -m \"...\"   then   ./mox.sh push")
