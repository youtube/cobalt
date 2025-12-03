#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Runs Cobalt code through Chromium presubmits"""

import os
import sys
import shutil
from typing import List

# Add location of depot_tools to find modules
if os.path.isdir("./third_party/depot_tools"):
  sys.path.append(os.path.abspath("./third_party/depot_tools"))
  import presubmit_support
  import presubmit_canned_checks
else:
  raise ModuleNotFoundError("third_party/depot_tools is not found")

# TODO: Revisit these checks this presubmit is actively being used
SKIPPED_PRESUBMIT_CHECKS = [
    "CheckAuthorizedAuthor",
    "CheckChangeWasUploaded",
    "CheckLicense",
    "CheckOwners",
    "CheckChangeHasBugFieldFromChange",
    "CheckForCommitObjects",
    # Currently ignores "NOLINT(build/header_guard)"
    "CheckForIncludeGuards",
    "CheckForTooLargeFiles",
    # This should be re-enabled when we have a good way to run chromium
    # precommits with local chromium PRESUBMIT.py files considered
    "CheckLongLines",
    "CheckOwnersFormat",
    "CheckOwnersOnCommit",
    "CheckOwnersOnUpload",
    "CheckPatchFormatted",
    "CheckSecurityOwners"
]

# --- Monkey-patching to remove Gerrit dependencies ---
# pylint: disable=protected-access
original_run_check_function = \
  presubmit_support.PresubmitExecuter._run_check_function

# pylint: enable=protected-access


# Define the patched version of _run_check_function to disable OWNERS checks
def filtered_run_check_function(self, function_name, context, sink,
                                presubmit_path):
  """A patched version that disables OWNERS checks which require Gerrit."""
  if function_name in SKIPPED_PRESUBMIT_CHECKS:
    # Return an empty list to mimic a successful check
    return []
  # Call the original function for all other checks
  return original_run_check_function(self, function_name, context, sink,
                                     presubmit_path)


presubmit_support.PresubmitExecuter._run_check_function = \
  filtered_run_check_function  # pylint: disable=protected-access


def no_op_check_function(*_args, **_kwargs):
  """A no-op version for check function."""
  return []


# Presubmit checks that are called from other checks can't be directly blocked
# with our monekypatched solution, so we also have to monkeypatch the original
# module. For now just check on presubmit_canned_functions by default
for check in SKIPPED_PRESUBMIT_CHECKS:
  if hasattr(presubmit_canned_checks, check):
    setattr(presubmit_canned_checks, check, no_op_check_function)


def get_file_changes_for_presubmit(file_paths):
  """
  Converts a list of file paths into the format expected by presubmit_support.
  Change. For now, it assumes all files are 'Modified'. A more advanced
  version could use git to determine the actual status (Added, Modified,
  Deleted).
  """
  # TODO: Determine what assigning proper change types would get us
  changes = []
  for path in file_paths:
    if os.path.exists(path):
      # Assume 'M' for any existing file passed by the hook.
      # 'A' would also work. 'D' would require more logic.
      changes.append(("M", path))
    else:
      raise FileNotFoundError(f"File for change not found: {path}")
  return changes


def main(argv: List[str]) -> int:
  """
  Main function to execute Chromium presubmit checks on files
  staged in pre-commit.
  """
  if shutil.which("gclient") is None:
    print("depot_tools doesn't seem to be installed.")
    return 1

  if not argv:
    print("No staged files to check.")
    return 0

  # --- Prepare the Change object ---
  repository_root = os.getcwd()
  changed_files = get_file_changes_for_presubmit(argv)
  change = presubmit_support.GitChange(
      "Pre-commit Change",
      "Files staged for commit.",
      repository_root,
      changed_files,
      None,
      None,
      "fake@user.com",
      upstream="origin",
      end_commit="HEAD")

  # --- Run the presubmit checks ---
  print(f"Running Chromium presubmit checks on {len(changed_files)} file(s)...")
  results = presubmit_support.DoPresubmitChecks(
      change=change,
      committing=True,
      verbose=0,
      default_presubmit=None,  # No default presubmit.
      may_prompt=False,
      gerrit_obj=None,  # Explicitly disable Gerrit integration
      json_output=None)
  if results:
    print("Presubmit checks found issues.")
    return 1
  else:
    print("All presubmit checks passed.")
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
