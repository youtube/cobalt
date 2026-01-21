#!/bin/bash
set -e

# Syncs 'starboard/contrib/rdk' changes from Cobalt (Source) to a standalone RDK repo (Dest).
# Usage: Follow the steps below manually or run sections of this script.

SOURCE_REPO="$HOME/chromium/src"
DEST_REPO="$HOME/rdk/upstream_repo"
SOURCE_REF="HEAD"

# --- Step 1: Export history from Source (Cobalt/Chromium) ---
# Creates a branch 'full-rdk-history' with only the RDK folder's history.
cd "$SOURCE_REPO"
git subtree split --prefix=starboard/contrib/rdk -b full-rdk-history "$SOURCE_REF"

# --- Step 2: Merge into Destination (RDK Repo) ---
# Pulls the exported history into the standalone repo.
cd "$DEST_REPO"

if ! git remote | grep -q "^local_chromium$"; then
  git remote add local_chromium "$SOURCE_REPO"
fi

git fetch local_chromium full-rdk-history
# Allow merging independent histories since the subtree was extracted from a different repo.
git merge --allow-unrelated-histories local_chromium/full-rdk-history -m "Merge RDK changes from Cobalt tree"

# --- Cleanup ---
cd "$SOURCE_REPO"
git branch -D full-rdk-history