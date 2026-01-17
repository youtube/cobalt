#!/bin/bash

# Syncs 'starboard/contrib/rdk' changes from Cobalt (Source) to a standalone RDK repo (Dest).
# Usage: Follow the steps below manually or run sections of this script.

# --- Step 1: Export history from Source (Cobalt/Chromium) ---
# Creates a branch 'full-rdk-history' with only the RDK folder's history.
cd ~/chromium/src
git subtree split --prefix=starboard/contrib/rdk -b full-rdk-history d7100a25583

# --- Step 2: Merge into Destination (RDK Repo) ---
# Pulls the exported history into the standalone repo.
cd ~/rdk/upstream_repo
git remote add local_chromium ~/chromium/src
git fetch local_chromium full-rdk-history
# Allow merging independent histories since the subtree was extracted from a different repo.
git merge --allow-unrelated-histories local_chromium/full-rdk-history
git commit -m "Merge RDK changes from Cobalt tree"

# --- Cleanup ---
cd ~/chromium/src
git branch -D full-rdk-history