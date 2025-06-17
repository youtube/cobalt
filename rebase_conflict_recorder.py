import os
import shutil
import subprocess
from git import Repo, InvalidGitRepositoryError, GitCommandError
from datetime import datetime

def get_conflicted_files(repo):
    """
    Identifies files that are currently in a conflicted (unmerged) state.
    Returns a list of file paths relative to the repository root.
    """
    conflicted_files = []
    # Using 'git status --porcelain' to get machine-readable status
    status_output = repo.git.status('--porcelain').splitlines()

    for line in status_output:
        # Expected formats for conflicted files:
        # UU path/to/file (Unmerged, both modified)
        # AA path/to/file (Added by both)
        # DD path/to/file (Deleted by both)
        # UD path/to/file (Unmerged, Deleted by one side, Modified by other)
        # DU path/to/file (Deleted by one side, Unmerged, Modified by other)
        if line.startswith(('UU ', 'AA ', 'DD ', 'UD ', 'DU ')):
            file_path = line[3:].strip() # Extract path after status code
            conflicted_files.append(file_path)
    return conflicted_files

def record_rebase_conflict():
    """
    Records the state of conflicted files during a git rebase operation,
    prompts the user to resolve, then records the resolved state and a patch.
    Finally, it stages the resolution and continues the rebase.
    """
    # --- Configuration ---
    # Define the base directory for storing rebase conflict recordings.
    # You can set this as an environment variable or hardcode it here.
    # If hardcoding, make sure to change it for your system.
    REBASE_DIR = os.environ.get("REBASEDIR")
    if not REBASE_DIR:
        print("ERROR: REBASEDIR environment variable is not set.")
        print("Please set it before running this script: export REBASEDIR=\"/path/to/your/rebase_recordings\"")
        return

    # Ensure the base recording directory exists
    os.makedirs(REBASE_DIR, exist_ok=True)

    try:
        repo = Repo(os.getcwd())
    except InvalidGitRepositoryError:
        print("ERROR: Not in a Git repository. Please run this script from within your Git repo.")
        return

    # --- 1. Get current commit hash and set up directories ---
    try:
        # **UPDATED LOGIC HERE:**
        # During rebase, HEAD is typically detached and REBASE_HEAD points to the commit being applied.
        # This is the most reliable way to get the commit that caused the conflict pause.
        commit_id = repo.git.rev_parse('REBASE_HEAD')
        
        # Fallback/alternative checks for clarity, but 'REBASE_HEAD' should be primary
        # If the rebase is interactive, it might also have specific files
        rebase_merge_path = os.path.join(repo.git_dir, 'rebase-merge')
        rebase_apply_path = os.path.join(repo.git_dir, 'rebase-apply')

        # If you need more specific context than REBASE_HEAD (e.g., the original commit SHA before any reword/edit)
        # you could read from these files, but for simply recording *this* conflict, REBASE_HEAD is sufficient.
        
        # Example of getting original commit from rebase-merge (less critical for this script's purpose)
        # if os.path.exists(os.path.join(rebase_merge_path, 'original-commit')):
        #     with open(os.path.join(rebase_merge_path, 'original-commit'), 'r') as f:
        #         original_commit_id = f.read().strip()
        # else:
        #     original_commit_id = commit_id # Fallback to REBASE_HEAD if no original-commit file

    except GitCommandError as e:
        if "unknown revision or path not in the working tree" in str(e) and "REBASE_HEAD" in str(e):
            print("ERROR: Not currently in a rebase operation (REBASE_HEAD not found).")
            print("Please run this script only when 'git rebase' has stopped on a conflict.")
            return
        else:
            raise # Re-raise other unexpected GitCommandErrors

    current_commit_obj = repo.commit(commit_id) # Get the commit object from the SHA
    current_commit_short_sha = current_commit_obj.hexsha[:7]
    current_commit_summary = current_commit_obj.summary

    print(f"Commit currently being processed (causing conflict): {current_commit_short_sha} ({current_commit_summary})")


    commit_record_dir = os.path.join(REBASE_DIR, current_commit_short_sha)
    conflict_dir = os.path.join(commit_record_dir, "conflict")
    resolved_dir = os.path.join(commit_record_dir, "resolved")
    patch_dir = os.path.join(commit_record_dir, "patch")

    os.makedirs(conflict_dir, exist_ok=True)
    os.makedirs(resolved_dir, exist_ok=True)
    os.makedirs(patch_dir, exist_ok=True)

    print(f"Recording to: {commit_record_dir}")
    print(f"Directories created for {current_commit_short_sha}.")

    # --- 2. Automatically find and list conflicted files ---
    conflicted_files = get_conflicted_files(repo)

    if not conflicted_files:
        print("WARNING: No conflicted files found by git status. Exiting.")
        print("This script should only be run when git rebase has reported conflicts.")
        return

    print("Found conflicted files:")
    for file_path in conflicted_files:
        print(f"  - {file_path}")

    # --- 3. Copy conflicted files to CONFLICT_DIR ---
    print("Copying conflicted states...")
    for file_path in conflicted_files:
        source_path = os.path.join(repo.working_dir, file_path)
        destination_path = os.path.join(conflict_dir, file_path)

        os.makedirs(os.path.dirname(destination_path), exist_ok=True)
        try:
            shutil.copy2(source_path, destination_path)
            print(f"  Copied {file_path} to {destination_path}")
        except FileNotFoundError:
            print(f"WARNING: File not found in working directory: {source_path}. Skipping.")
        except Exception as e:
            print(f"ERROR: Could not copy {source_path} to {destination_path}: {e}")

    # --- 4. MANUAL STEP: User resolves conflicts ---
    print("\n" + "#" * 70)
    print("!!! MANUAL INTERVENTION REQUIRED: RESOLVE CONFLICTS NOW !!!")
    print("Open each of the following files in your editor and resolve the conflicts:")
    for file_path in conflicted_files:
        print(f"  - {os.path.join(repo.working_dir, file_path)}") # Print full path for clarity
    print("Once you have resolved ALL conflicts in your working directory,")
    print("and saved the changes, press Enter to continue this script.")
    print("#" * 70)
    input("Press Enter after you have finished resolving all conflicts... ")

    # --- 5. Copy resolved files to RESOLVED_DIR ---
    print("Copying resolved states...")
    for file_path in conflicted_files:
        source_path = os.path.join(repo.working_dir, file_path)
        destination_path = os.path.join(resolved_dir, file_path)

        os.makedirs(os.path.dirname(destination_path), exist_ok=True)
        try:
            shutil.copy2(source_path, destination_path)
            print(f"  Copied {file_path} to {destination_path}")
        except FileNotFoundError:
            print(f"WARNING: File not found in working directory after resolution: {source_path}. Skipping.")
        except Exception as e:
            print(f"ERROR: Could not copy {source_path} to {destination_path}: {e}")

    # --- 6. Create resolution patch for each conflicted file ---
    print("Creating resolution patches...")
    for file_path in conflicted_files:
        conflicted_file_path_in_record = os.path.join(conflict_dir, file_path)
        resolved_file_path_in_record = os.path.join(resolved_dir, file_path)
        
        # Ensure the files exist before attempting to diff
        if not os.path.exists(conflicted_file_path_in_record) or not os.path.exists(resolved_file_path_in_record):
            print(f"  WARNING: Skipping patch for {file_path} as either conflicted or resolved state file is missing.")
            continue

        patch_filename = f"{os.path.basename(file_path).split('.')[0]}_resolution_{datetime.now().strftime('%Y%m%d%H%M%S')}.patch"
        patch_path = os.path.join(patch_dir, patch_filename)

        try:
            with open(patch_path, 'w') as f:
                result = subprocess.run(  # Store the result
                    ['diff', '-u', conflicted_file_path_in_record, resolved_file_path_in_record],
                    stdout=f,
                    # text=True, # text=True is good for stdout, but not strictly necessary for file redirection
                    check=False # <--- CRITICAL CHANGE: Do NOT raise error for non-zero exit codes
                )
            
            # Now, manually check the return code
            if result.returncode == 0:
                print(f"  WARNING: Patch for {file_path} is empty (files are identical). No changes detected from conflicted to resolved state.")
                print("  This might mean the conflict was trivial and resolved by deleting markers without content changes,")
                print("  or an error occurred in your manual resolution.")
            elif result.returncode == 1:
                # This is the expected case: diff found differences and successfully wrote them
                if os.path.getsize(patch_path) == 0:
                     print(f"  WARNING: Patch for {file_path} is unexpectedly empty despite diff returning 1. Check content.")
                else:
                    print(f"  Created patch {patch_path}")
            else: # result.returncode == 2 or other unexpected code
                print(f"ERROR: Failed to create patch for {file_path}. 'diff' command returned an unexpected exit status {result.returncode}.")
                print(f"  Command: {' '.join(result.args)}") # Print the actual command that failed
                print(f"  Stderr: {result.stderr.decode() if result.stderr else 'N/A'}")

        except FileNotFoundError: # If 'diff' command itself is not found
            print(f"ERROR: 'diff' command not found. Please ensure it's installed and in your system's PATH.")
        except Exception as e:
            print(f"ERROR: An unexpected error occurred while creating patch for {file_path}: {e}")

    # --- 7. Stage resolved files in Git and continue rebase ---
    print("Stage resolved files in Git... and then")
    print("Please continue with git rebase --continue")

if __name__ == "__main__":
    record_rebase_conflict()
