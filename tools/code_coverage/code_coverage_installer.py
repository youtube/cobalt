#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""CodeCoverageInstaller: Verifies and installs code coverage dependencies.

Automates the verification of:
1. Chromium checkout.
2. LLVM coverage tools (llvm-cov, llvm-profdata).
3. Recipes code (chromium/tools/build).
4. Service code (infra/infra).
"""

import argparse
import os
import pathlib
import subprocess
import sys
import textwrap

SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]


def print_indented(text, level=1, file=None):
  """Prints text with indentation (2 spaces per level)."""
  if file is None:
    file = sys.stdout
  indent = '  ' * level
  for line in text.splitlines():
    print(f"{indent}{line}", file=file)


def run_command(args, cwd=None):
  """Helper to run commands and print stdout/stderr."""
  result = subprocess.run(
      args,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      cwd=cwd,
      check=False,
  )
  if result.stdout:
    print_indented(result.stdout, level=2)
  return result.returncode


def verify_chromium_checkout(src_root):
  """Step 1: Confirms that a valid Chromium checkout exists."""
  print('Verifying Chromium checkout... ', end='', flush=True)
  try:
    deps_path = src_root / 'DEPS'
    if deps_path.exists():
      print('OK')
      return True
    print('FAIL')
    print_indented(
        'Error: Could not find DEPS file. Not a valid Chromium checkout.',
        file=sys.stderr)
  except Exception as e:
    print('FAIL')
    print_indented(f'Error verifying checkout: {e}', file=sys.stderr)
  return False


def verify_llvm_tools(src_root):
  """Step 2: Confirms that LLVM coverage tools are present."""
  print('Verifying LLVM coverage tools... ', end='', flush=True)
  try:
    llvm_bin_dir = (src_root / 'third_party' / 'llvm-build' /
                    'Release+Asserts' / 'bin')
    llvm_cov = llvm_bin_dir / 'llvm-cov'
    llvm_profdata = llvm_bin_dir / 'llvm-profdata'

    if llvm_cov.exists() and llvm_profdata.exists():
      print('OK')
      return True

    print('FAIL')
    print_indented(textwrap.dedent("""\
            Error: LLVM coverage tools (llvm-cov, llvm-profdata) are missing.
            To install them, add the following to your .gclient configuration:
              "custom_vars": {
                "checkout_clang_coverage_tools": True,
              }
            And then run: gclient runhooks"""),
                   file=sys.stderr)
  except Exception as e:
    print('FAIL')
    print_indented(f'Error verifying LLVM tools: {e}', file=sys.stderr)
  return False


def verify_recipes(infra_dir):
  """Step 3: Confirms that recipe codebase is present."""
  print('Verifying code coverage recipe code... ', end='', flush=True)
  recipe_module_path = (infra_dir / 'build' / 'recipes' / 'recipe_modules' /
                        'code_coverage')
  if recipe_module_path.exists():
    print('OK')
    return True
  print('FAIL')
  return False


def verify_service(infra_dir):
  """Step 4: Confirms that coverage service codebase is present."""
  print('Verifying code coverage service code... ', end='', flush=True)
  service_path = infra_dir / 'infra' / 'appengine' / 'findit'
  if service_path.exists():
    print('OK')
    return True
  print('FAIL')
  return False


def setup_infra(infra_dir):
  """Sets up the infra_superproject in the specified directory."""
  print_indented(f'Setting up infra_superproject in {infra_dir}...')
  if not infra_dir.exists():
    infra_dir.mkdir(parents=True)

  gclient_file = infra_dir / '.gclient'
  if gclient_file.exists():
    print_indented(
        'Existing gclient configuration found. Running gclient sync...')
    code = run_command(['gclient', 'sync'], cwd=infra_dir)
    if code == 0:
      print_indented('gclient sync completed successfully.')
      return True
    print_indented(f'Error: gclient sync failed with exit code {code}.',
                   file=sys.stderr)
  else:
    print_indented('Running fetch infra_superproject...')
    code = run_command(['fetch', 'infra_superproject'], cwd=infra_dir)
    if code == 0:
      print_indented('fetch infra_superproject completed successfully.')
      return True
    print_indented(
        f'Error: fetch infra_superproject failed with exit code {code}.',
        file=sys.stderr)
  return False


def main():
  parser = argparse.ArgumentParser(
      description=
      'Verifies and initializes Code Coverage development environment.')
  parser.add_argument(
      '--infra-dir',
      required=True,
      type=pathlib.Path,
      help='Path to the directory containing (or to checkout) chrome infra '
      'repositories.')
  args = parser.parse_args()

  infra_dir = args.infra_dir.resolve()

  if not verify_chromium_checkout(SRC_ROOT):
    sys.exit(1)

  if not verify_llvm_tools(SRC_ROOT):
    sys.exit(1)

  recipes_ok = verify_recipes(infra_dir)
  service_ok = verify_service(infra_dir)

  if not (recipes_ok and service_ok):
    if not setup_infra(infra_dir):
      sys.exit(1)
    # Re-verify
    if not verify_recipes(infra_dir) or not verify_service(infra_dir):
      print_indented('Error: Verification failed after setup.', file=sys.stderr)
      sys.exit(1)

  print('\nCode Coverage environment successfully verified and initialized')


if __name__ == '__main__':
  main()
