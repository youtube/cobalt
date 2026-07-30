---
name: remove-unused-includes
description: Look through a given folder to find and safely remove unused includes specifically for Clang-compiled code (using clang-include-cleaner), verifying that referencing targets compile.
---

# 🧹 Remove Unused Includes Skill

This skill provides a workflow and script to look through a given folder,
identify unused includes using `clang-include-cleaner`, and safely remove them
only if the affected targets continue to compile successfully.

## Prerequisites

1. **Compilation Database (`compile_commands.json`)**: Ensure your compilation
   database is up-to-date. If not, generate a fresh compilation database using:
   ```bash
   python3 tools/clang/scripts/generate_compdb.py -p out/Default -o compile_commands.json
   ```

## Workflow Steps

### 1. Run the Cleanup Script

Run the automated cleanup script located inside this skill's directory (resolve
the absolute `<skill_directory_path>` from the skill metadata):

```bash
python3 <skill_directory_path>/scripts/remove_unused_includes.py --src-root . --folder <relative_or_absolute_folder_path>
```

### 2. Verify Compilation

The script automatically:

- Finds all `.cc` files in the specified folder.
- Detects unused includes using `clang-include-cleaner`.
- Locates all referencing GN targets for each modified file via `gn refs`.
- Edits the file and compiles each referencing target.
- Reverts the edit if any compilation errors occur.

### 3. Review Git Status

Once execution is complete, verify the final changes using git:

```bash
git status
git diff
```
