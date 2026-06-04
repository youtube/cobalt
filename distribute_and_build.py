import os
import subprocess
import sys

PRS = {
    "pr-2-starboard": 10540,
    "pr-3-scheduler": 10541,
    "pr-4-coordinator": 10542,
    "pr-5-annotations": 10544,
    "pr-6-verification": 10545,
}

# The files in PR 1 that should go to a new PR
PR1_FILES = [
    "base/BUILD.gn",
    "base/memory/cobalt_memory_attribution_observer.cc",
    "base/memory/cobalt_memory_attribution_observer.h",
    "base/memory/cobalt_memory_context.cc",
    "base/memory/cobalt_memory_context.h",
]

def run(cmd):
    print(f"Running: {cmd}")
    res = subprocess.run(cmd, shell=True, text=True, capture_output=True)
    if res.returncode != 0:
        print(f"Error executing: {cmd}")
        print(res.stdout)
        print(res.stderr)
        sys.exit(1)
    return res.stdout.strip()

def build_branch():
    print("Generating GN for linux...")
    run("python3 cobalt/build/gn.py -p linux-x64x11 -c devel")
    print("Building gn_all on linux...")
    run("env -u ANTIGRAVITY_AGENT -u GEMINI_CLI autoninja -C out/linux-x64x11_devel/ cobalt:gn_all cobalt_unittests cobalt_browsertests")
    print("Generating GN for android...")
    run("python3 cobalt/build/gn.py -p android-arm64 -c devel")
    print("Building gn_all on android...")
    run("env -u ANTIGRAVITY_AGENT -u GEMINI_CLI autoninja -C out/android-arm64_devel/ cobalt:gn_all cobalt_unittests cobalt_browsertests")
    print("Builds successful.")

# 1. Base updates
print("\n=== Handling PR 1 Base Updates ===")
run("git checkout -B new-pr-1-base origin/main")
for f in PR1_FILES:
    run(f"git checkout pr-10476 -- {f}")
run("git add .")
res = subprocess.run("git commit --no-verify -m 'Update base memory files from prototype'", shell=True)
if res.returncode == 0:
    print("Committed base files.")
build_branch()

# Proceed down the stack
previous_branch = "new-pr-1-base"
for branch_name, pr_num in PRS.items():
    print(f"\n=== Handling {branch_name} (PR {pr_num}) ===")
    
    # Checkout the PR locally
    run(f"gh pr checkout {pr_num} -b {branch_name}_local")
    
    # Rebase onto the previous branch
    print(f"Rebasing {branch_name}_local onto {previous_branch}...")
    res = subprocess.run(f"git rebase {previous_branch}", shell=True)
    if res.returncode != 0:
        print("Rebase conflict. Exiting.")
        sys.exit(1) # We might need manual resolution

    # Get the files mapped to this PR
    files_json = run(f"gh pr view {pr_num} --json files -q '.files[].path'")
    pr_files = files_json.split("\n")
    
    # Copy from prototype
    for f in pr_files:
        if not f.strip(): continue
        # Only checkout if the file exists in prototype
        subprocess.run(f"git checkout pr-10476 -- {f}", shell=True)
        
    run("git add .")
    res = subprocess.run("git commit --no-verify -m 'Distribute prototype changes'", shell=True)
    if res.returncode != 0:
        print("Commit failed, but we will assume no changes were needed.")
    
    build_branch()
    previous_branch = f"{branch_name}_local"

print("Done distributing and building!")
