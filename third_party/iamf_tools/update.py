#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates iamf-tools to a specific commit.

This script downloads the iamf-tools source archive and extracts it,
excluding heavy directories (like testdata and gh_pages) to keep the
checkout size small. It updates README.chromium with the new revision.
"""

import argparse
import os
import re
import subprocess
import tempfile
import shutil
import sys

# The GitHub repository URL.
REPO_URL = 'https://github.com/AOMediaCodec/iamf-tools'

# Returns True if the file should be extracted to the Chromium workspace.
def should_include(rel_path):
    # Always include LICENSE and PATENTS.
    filename = os.path.basename(rel_path)
    if filename in ('LICENSE', 'PATENTS'):
        return True

    # Only include files under the 'iamf/' source directory.
    if not rel_path.startswith('iamf/'):
        return False

    # Exclude directories we don't need in Chromium.
    excluded_dirs = (
        'iamf/cli/adm_to_user_metadata/',
        'iamf/cli/ambisonic_encoder/',
        'iamf/cli/itu_1770_4/',
        'iamf/cli/proto_conversion/',
        'iamf/cli/user_metadata_builder/',
    )
    if rel_path.startswith(excluded_dirs):
        return False

    # Exclude tests and test data.
    # We want to keep the decoder fuzz test, but exclude other tests.
    if rel_path in (
        'iamf/api/decoder/tests/iamf_decoder_fuzz_test.cc',
        'iamf/cli/tests/portable/get_test_path.h',
        'iamf/cli/tests/iamf_status_adl.h',
    ):
        return True

    # Check for test directories or files.
    normalized_path = rel_path.replace(os.sep, '/')
    path_parts = normalized_path.split('/')
    if 'test' in path_parts or 'tests' in path_parts or 'testdata' in path_parts:
        return False

    # Also exclude files ending with _test
    name_without_ext, ext = os.path.splitext(filename)
    if name_without_ext.endswith('_test') or name_without_ext.endswith('_unittest'):
        return False

    # Only include C/C++ source and header files.
    return ext in ('.h', '.cc', '.c')

def get_latest_commit():
    """Fetches the latest commit hash from the remote repository."""
    print(f'Fetching latest commit from {REPO_URL}...')
    try:
        output = subprocess.check_output(
            ['git', 'ls-remote', REPO_URL + '.git', 'HEAD'],
            universal_newlines=True
        )
        # Output is typically: "<hash>\tHEAD"
        return output.split()[0]
    except Exception as e:
        print(f"Failed to get latest commit: {e}")
        sys.exit(1)

def update_readme(new_commit):
    """Updates the Revision line in README.chromium."""
    readme_path = os.path.join(os.path.dirname(__file__), 'README.chromium')
    if not os.path.exists(readme_path):
        print(f"WARNING: {readme_path} not found.")
        return

    with open(readme_path, 'r') as f:
        content = f.read()

    new_content = re.sub(
        r'^Revision:\s+[0-9a-fA-F]+',
        f'Revision: {new_commit}',
        content,
        flags=re.MULTILINE
    )

    if new_content != content:
        with open(readme_path, 'w') as f:
            f.write(new_content)
        print(f'Updated README.chromium with Revision: {new_commit}')
    else:
        print('README.chromium revision is already up to date.')

def main():
    parser = argparse.ArgumentParser(description='Update iamf-tools.')
    parser.add_argument('--revision', help='The commit hash to update to. Defaults to latest HEAD.', default=None)
    args = parser.parse_args()

    commit = args.revision
    if not commit:
        commit = get_latest_commit()

    with tempfile.TemporaryDirectory() as temp_dir:
        print(f'Cloning {REPO_URL} into temporary directory...')
        try:
            subprocess.check_call(['git', 'clone', REPO_URL, temp_dir])
            subprocess.check_call(['git', 'checkout', commit], cwd=temp_dir)
        except Exception as e:
            print(f"Failed to clone or checkout commit {commit}: {e}")
            sys.exit(1)

        print('Extracting files (applying exclusions)...')
        src_dir = os.path.join(os.path.dirname(__file__), 'src')
        if os.path.exists(src_dir):
            shutil.rmtree(src_dir)

        extracted_count = 0
        for root, dirs, files in os.walk(temp_dir):
            if '.git' in dirs:
                dirs.remove('.git')
            for file in files:
                abs_path = os.path.join(root, file)
                rel_path = os.path.relpath(abs_path, temp_dir)
                if should_include(rel_path):
                    target_path = os.path.join(src_dir, rel_path)
                    os.makedirs(os.path.dirname(target_path), exist_ok=True)
                    shutil.copy2(abs_path, target_path)
                    extracted_count += 1

        print(f'Update complete. Extracted {extracted_count} files.')
        update_readme(commit)

if __name__ == '__main__':
    main()