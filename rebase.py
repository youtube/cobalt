import os
import shutil
import subprocess
import glob
import logging
from git import Repo, InvalidGitRepositoryError, GitCommandError
from datetime import datetime

# --- Basic Logging Setup ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def get_conflicted_files(repo):
    """
    Identifies files that are currently in a conflicted (unmerged) state.
    Returns a list of file paths relative to the repository root.
    """
    conflicted_files = []
    status_output = repo.git.status('--porcelain').splitlines()
    for line in status_output:
        if line.startswith(('UU ', 'AA ', 'DD ', 'UD ', 'DU ')):
            file_path = line[3:].strip()
            conflicted_files.append(file_path)
    return conflicted_files

def find_existing_patch(patch_dir, file_path):
    """
    Finds an existing patch for a given file, accommodating different naming conventions.
    It looks for both new (`{filename}_resolution.patch`) and old (`{filename}_resolution_*.patch`) formats.
    """
    base_patch_name = f"{os.path.basename(file_path).split('.')[0]}_resolution"
    simple_patch_path = os.path.join(patch_dir, f"{base_patch_name}.patch")
    if os.path.exists(simple_patch_path):
        return simple_patch_path
    
    glob_pattern = os.path.join(patch_dir, f"{base_patch_name}_*.patch")
    patches = glob.glob(glob_pattern)
    if patches:
        return patches[0]
        
    return None

def record_rebase_conflict():
    """
    Records the state of conflicted files during a git rebase operation,
    prompts the user to resolve, then records the resolved state and a patch.
    Finally, it stages the resolution and continues the rebase.
    """
    REBASE_DIR = os.environ.get("REBASEDIR")
    if not REBASE_DIR:
        logging.error("REBASEDIR environment variable is not set.")
        return

    os.makedirs(REBASE_DIR, exist_ok=True)

    try:
        repo = Repo(os.getcwd())
    except InvalidGitRepositoryError:
        logging.error("Not in a Git repository.")
        return

    try:
        commit_id = repo.git.rev_parse('REBASE_HEAD')
    except GitCommandError as e:
        if "REBASE_HEAD" in str(e):
            logging.error("Not currently in a rebase operation.")
            return
        else:
            raise

    current_commit_obj = repo.commit(commit_id)
    current_commit_short_sha = current_commit_obj.hexsha[:7]
    current_commit_summary = current_commit_obj.summary
    logging.info(f"Commit causing conflict: {current_commit_short_sha} ({current_commit_summary})")

    commit_record_dir = os.path.join(REBASE_DIR, current_commit_short_sha)
    conflict_dir = os.path.join(commit_record_dir, "conflict")
    resolved_dir = os.path.join(commit_record_dir, "resolved")
    patch_dir = os.path.join(commit_record_dir, "patch")

    os.makedirs(conflict_dir, exist_ok=True)
    os.makedirs(resolved_dir, exist_ok=True)
    os.makedirs(patch_dir, exist_ok=True)

    logging.info(f"Recording to: {commit_record_dir}")

    conflicted_files = get_conflicted_files(repo)
    if not conflicted_files:
        logging.warning("No conflicted files found. Exiting.")
        return

    logging.info("Found conflicted files:")
    for file_path in conflicted_files:
        logging.info(f"  - {file_path}")

    logging.info("Copying conflicted states...")
    for file_path in conflicted_files:
        source_path = os.path.join(repo.working_dir, file_path)
        destination_path = os.path.join(conflict_dir, file_path)
        os.makedirs(os.path.dirname(destination_path), exist_ok=True)
        try:
            shutil.copy2(source_path, destination_path)
        except FileNotFoundError:
            logging.warning(f"File not found: {source_path}. Skipping.")

    logging.info("Checking for existing resolution patches...")
    patches_to_apply = {}
    for file_path in conflicted_files:
        patch_path = find_existing_patch(patch_dir, file_path)
        if patch_path:
            patches_to_apply[file_path] = patch_path

    auto_apply_succeeded = False
    if patches_to_apply:
        logging.info("Found existing patches. Attempting to apply them...")
        all_patches_applied_cleanly = True
        for file_path, patch_path in patches_to_apply.items():
            logging.info(f"  -> Applying patch for {file_path}...")
            try:
                result = subprocess.run(
                    ['patch', file_path, '-i', patch_path],
                    capture_output=True, text=True, cwd=repo.working_dir, check=False
                )
                if result.returncode == 0:
                    logging.info(f"     SUCCESS: Patch applied cleanly.")
                else:
                    logging.error(f"     Patch for {file_path} did not apply cleanly.")
                    logging.error(f"     Stderr: {result.stderr.strip()}")
                    all_patches_applied_cleanly = False
                    break
            except Exception as e:
                logging.error(f"     Exception while applying patch for {file_path}: {e}")
                all_patches_applied_cleanly = False
                break

        if all_patches_applied_cleanly:
            logging.info("!!! AUTO-PATCH SUCCESSFUL !!!")
            logging.info("Press Enter to continue and record this resolution.")
            input("Press Enter to continue... ")
            auto_apply_succeeded = True
        else:
            logging.error("!!! AUTO-PATCH FAILED !!!")
            logging.info("Restoring original conflicted state.")
            for file_path in conflicted_files:
                backup_path = os.path.join(conflict_dir, file_path)
                target_path = os.path.join(repo.working_dir, file_path)
                if os.path.exists(backup_path):
                    shutil.copy2(backup_path, target_path)
            logging.info("Please resolve conflicts manually.")

    if not auto_apply_succeeded:
        logging.info("!!! MANUAL INTERVENTION REQUIRED !!!")
        logging.info("Resolve conflicts, then press Enter to continue.")
        input("Press Enter after resolving conflicts... ")

    logging.info("Copying resolved states...")
    for file_path in conflicted_files:
        source_path = os.path.join(repo.working_dir, file_path)
        destination_path = os.path.join(resolved_dir, file_path)
        os.makedirs(os.path.dirname(destination_path), exist_ok=True)
        try:
            shutil.copy2(source_path, destination_path)
        except FileNotFoundError:
            logging.warning(f"File not found after resolution: {source_path}. Skipping.")

    logging.info("Creating resolution patches...")
    for file_path in conflicted_files:
        conflicted_file_path_in_record = os.path.join(conflict_dir, file_path)
        resolved_file_path_in_record = os.path.join(resolved_dir, file_path)
        
        if not os.path.exists(conflicted_file_path_in_record) or not os.path.exists(resolved_file_path_in_record):
            logging.warning(f"  Skipping patch for {file_path}, missing state file.")
            continue

        patch_filename = f"{os.path.basename(file_path).split('.')[0]}_resolution.patch"
        patch_path = os.path.join(patch_dir, patch_filename)

        try:
            with open(patch_path, 'w') as f:
                result = subprocess.run(
                    ['diff', '-u',
                     '--label', file_path,
                     '--label', file_path,
                     conflicted_file_path_in_record,
                     resolved_file_path_in_record],
                    stdout=f, check=False
                )
            
            if result.returncode in [0, 1]:
                if os.path.getsize(patch_path) > 0:
                    logging.info(f"  Created patch {patch_path}")
                else:
                    os.remove(patch_path) # Remove empty patch file
                    logging.info(f"  Patch for {file_path} was empty and removed.")
            else:
                logging.error(f"Failed to create patch for {file_path}. 'diff' returned {result.returncode}.")

        except Exception as e:
            logging.error(f"An unexpected error occurred while creating patch for {file_path}: {e}")

    logging.info("Resolution recorded. Stage files and continue with 'git rebase --continue'.")
