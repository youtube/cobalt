# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared code for //agents presubmit-related code."""

import pathlib

AGENTS_DIR = pathlib.Path(__file__).parent
CHROMIUM_SRC_DIR = AGENTS_DIR.parent


def get_agents_python_path_entries() -> list[pathlib.Path]:
    """Gets a list of paths to add to PYTHONPATH."""
    return [
        # Needed so that the code in this directory can import neighboring code
        # while maintaining compatibility with pytype. The hyphens in skill
        # names affect pytype's ability to resolve paths for relative imports.
        (CHROMIUM_SRC_DIR / 'agents' / 'skills' / 'analyzing-sql-traces' /
         'scripts'),
        # Python code under //agents uses (or should use) fully qualified
        # imports relative to the Chromium src directory wherever possible.
        CHROMIUM_SRC_DIR,
    ]


def get_agents_env(input_api):
    """Gets the common environment for running agents tests."""
    python_path = [str(p) for p in get_agents_python_path_entries()]
    existing_python_path = input_api.environ.get('PYTHONPATH', '')
    if existing_python_path:
        python_path.append(existing_python_path)
    env = dict(input_api.environ)
    env.update({
        'PYTHONPATH': input_api.os_path.pathsep.join(python_path),
        'PYTHONDONTWRITEBYTECODE': '1',
    })
    return env
