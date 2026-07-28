"""
Stages everything and commits it.

  ./mox.sh commit -m "message"    Stage all changes and commit
  ./mox.sh commit --dry-run       Show exactly what would be committed
  ./mox.sh commit                 Prompts for the message

Deliberately does NOT build first. A commit is a save point, and refusing
to save broken work is the opposite of what a save point is for - run
./mox.sh build yourself when you want that guarantee.
"""

import argparse
import sys

import moxbackup as mb

# git status --porcelain codes, expanded for humans.
STATUS_LABELS = {
    "??": "new",
    "A": "added",
    "M": "modified",
    "D": "deleted",
    "R": "renamed",
    "C": "copied",
    "U": "conflict",
}


def Summarize():
    """Prints a grouped summary. Returns the number of changed entries."""
    out = mb.GitOut("status", "--porcelain")
    if not out:
        return 0

    lines = out.splitlines()
    groups = {}
    for line in lines:
        code = line[:2].strip() or line[:2]
        label = STATUS_LABELS.get(code, STATUS_LABELS.get(code[:1], code))
        groups.setdefault(label, []).append(line[3:])

    for label in sorted(groups):
        paths = groups[label]
        print(f"  {label:<10} {len(paths)}")
        for path in sorted(paths)[:8]:
            print(f"    {path}")
        if len(paths) > 8:
            print(f"    ... and {len(paths) - 8} more")

    return len(lines)


if __name__ == "__main__":
    p = argparse.ArgumentParser(prog="mox commit", allow_abbrev=False)
    p.add_argument("-m", "--message", help="Commit message")
    p.add_argument("--dry-run", action="store_true", help="Show what would be committed, change nothing")
    args = p.parse_args()

    mb.Section("Working tree")
    changed = Summarize()

    if changed == 0:
        print("  nothing to commit - working tree is clean")
        sys.exit(0)

    if args.dry_run:
        print()
        print(f"{changed} entries would be staged and committed. Nothing was changed.")
        sys.exit(0)

    if not mb.RequireIdentity():
        sys.exit(1)

    message = args.message
    if not message:
        try:
            message = input("\nCommit message: ").strip()
        except KeyboardInterrupt:
            print()
            sys.exit(130)
    if not message:
        mb.Fail("empty commit message")
        sys.exit(1)

    mb.Section("Committing")
    # -A so deletions and renames are picked up, not just new/modified files.
    if mb.Git("add", "-A").returncode != 0:
        mb.Fail("git add failed")
        sys.exit(1)

    result = mb.Git("commit", "-m", message, capture=False)
    if result.returncode != 0:
        mb.Fail("git commit failed")
        sys.exit(result.returncode)

    mb.Ok(mb.GitOut("log", "--oneline", "-1"))
    print()
    print("Backed up nowhere yet - run ./mox.sh push")
