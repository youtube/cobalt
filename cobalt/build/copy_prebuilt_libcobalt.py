#!/usr/bin/env python3
#
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#
"""Helper script to copy and decompress a prebuilt libcobalt binary.

Supports uncompressed shared libraries (.so), LZ4 compressed files (.lz4),
and Zstandard compressed files (.zstd, .zst). Resolves absolute,
repository-relative, and build-directory-relative paths.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys


def resolve_path(input_path, repo_root=None, build_dir=None):
  """Resolves the input path against candidate directories.

  Args:
    input_path: Path to the prebuilt libcobalt file.
    repo_root: Optional path to the repository root.
    build_dir: Optional path to the build directory.

  Returns:
    The resolved absolute or existing relative path.

  Raises:
    FileNotFoundError: If the file cannot be located in any candidate path.
  """
  candidates = []

  # Expand user home if present (e.g. ~/path)
  expanded_path = os.path.expanduser(input_path)

  if expanded_path.startswith('//'):
    # Repository-relative GN path (e.g. //path/to/libcobalt.so)
    stripped = expanded_path[2:]
    if repo_root:
      candidates.append(os.path.join(repo_root, stripped))
    candidates.append(os.path.abspath(stripped))
  elif os.path.isabs(expanded_path):
    candidates.append(expanded_path)
  else:
    # Relative path: check cwd, build_dir, repo_root
    candidates.append(os.path.abspath(expanded_path))
    if build_dir:
      candidates.append(os.path.abspath(os.path.join(build_dir, expanded_path)))
    if repo_root:
      candidates.append(os.path.abspath(os.path.join(repo_root, expanded_path)))

  for candidate in candidates:
    if os.path.isfile(candidate):
      return candidate

  raise FileNotFoundError(f"Prebuilt libcobalt file not found: '{input_path}'. "
                          f'Checked candidate locations: {candidates}')


def decompress_lz4(input_path, output_path):
  """Decompresses an LZ4 file to output_path using Python library or CLI."""
  os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
  # Try python lz4.frame module
  have_module = False
  try:
    import lz4.frame  # pylint: disable=import-outside-toplevel
    have_module = True
  except ImportError:
    pass

  if have_module:
    try:
      with lz4.frame.open(
          input_path, mode='rb') as f_in, open(
              output_path, mode='wb') as f_out:
        shutil.copyfileobj(f_in, f_out, length=64 * 1024)
      return
    except Exception as e:
      raise RuntimeError(
          f"Corrupt or invalid LZ4 archive '{input_path}': {e}") from e

  # Try lz4 / unlz4 CLI tool
  lz4_cmd = shutil.which('lz4')
  if lz4_cmd:
    try:
      subprocess.run([lz4_cmd, '-d', '-f', input_path, output_path],
                     check=True,
                     capture_output=True,
                     text=True)
      return
    except subprocess.CalledProcessError as e:
      err_msg = e.stderr.strip() if e.stderr else str(e)
      raise RuntimeError(
          f"Corrupt or invalid LZ4 archive '{input_path}': {err_msg}") from e

  unlz4_cmd = shutil.which('unlz4')
  if unlz4_cmd:
    try:
      with open(output_path, 'wb') as f_out:
        subprocess.run([unlz4_cmd, '-c', input_path],
                       stdout=f_out,
                       stderr=subprocess.PIPE,
                       check=True,
                       text=True)
      return
    except subprocess.CalledProcessError as e:
      err_msg = e.stderr.strip() if e.stderr else str(e)
      raise RuntimeError(
          f"Corrupt or invalid LZ4 archive '{input_path}': {err_msg}") from e

  raise RuntimeError(
      f"Failed to decompress LZ4 file '{input_path}': neither the python 'lz4' "
      'module nor the system command-line tool (lz4 / unlz4) is available.')


def decompress_zstd(input_path, output_path):
  """Decompresses a Zstandard file to output_path using Python or CLI."""
  os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
  # Try python zstandard module
  have_zstandard = False
  try:
    import zstandard  # pylint: disable=import-outside-toplevel
    have_zstandard = True
  except ImportError:
    pass

  if have_zstandard:
    try:
      dctx = zstandard.ZstdDecompressor()
      with open(input_path, 'rb') as f_in, open(output_path, 'wb') as f_out:
        dctx.copy_stream(f_in, f_out, read_size=64 * 1024, write_size=64 * 1024)
      return
    except Exception as e:
      raise RuntimeError(
          f"Corrupt or invalid Zstandard archive '{input_path}': {e}") from e

  # Try python zstd module
  have_zstd = False
  try:
    import zstd  # pylint: disable=import-outside-toplevel
    have_zstd = True
  except ImportError:
    pass

  if have_zstd:
    try:
      with open(input_path, 'rb') as f_in, open(output_path, 'wb') as f_out:
        f_out.write(zstd.decompress(f_in.read()))
      return
    except Exception as e:
      raise RuntimeError(
          f"Corrupt or invalid Zstandard archive '{input_path}': {e}") from e

  # Try zstd / unzstd CLI tool
  zstd_cmd = shutil.which('zstd')
  if zstd_cmd:
    try:
      subprocess.run([zstd_cmd, '-d', '-f', input_path, '-o', output_path],
                     check=True,
                     capture_output=True,
                     text=True)
      return
    except subprocess.CalledProcessError as e:
      err_msg = e.stderr.strip() if e.stderr else str(e)
      raise RuntimeError(
          f"Corrupt or invalid Zstandard archive '{input_path}': {err_msg}"
      ) from e

  unzstd_cmd = shutil.which('unzstd')
  if unzstd_cmd:
    try:
      with open(output_path, 'wb') as f_out:
        subprocess.run([unzstd_cmd, '-c', input_path],
                       stdout=f_out,
                       stderr=subprocess.PIPE,
                       check=True,
                       text=True)
      return
    except subprocess.CalledProcessError as e:
      err_msg = e.stderr.strip() if e.stderr else str(e)
      raise RuntimeError(
          f"Corrupt or invalid Zstandard archive '{input_path}': {err_msg}"
      ) from e

  raise RuntimeError(
      f"Failed to decompress Zstandard file '{input_path}': neither python "
      "'zstandard'/'zstd' module nor the system command-line tool "
      '(zstd / unzstd) is available.')


def copy_or_decompress_prebuilt(input_path,
                                output_path,
                                repo_root=None,
                                build_dir=None,
                                unstripped_output=None):
  """Processes prebuilt libcobalt and writes libcobalt.so atomically.

  Args:
    input_path: Path to prebuilt libcobalt (.so, .lz4, .zstd, .zst).
    output_path: Destination path for libcobalt.so.
    repo_root: Optional repository root path.
    build_dir: Optional build directory path.
    unstripped_output: Optional destination path for unstripped libcobalt.so.
  """
  resolved_input = resolve_path(input_path, repo_root, build_dir)

  os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
  tmp_output = output_path + '.tmp'

  filename = os.path.basename(resolved_input).lower()
  is_lz4 = filename.endswith('.lz4')
  is_zstd = filename.endswith('.zstd') or filename.endswith('.zst')
  is_so = bool(re.search(r'\.so(\.\d+)*$', filename))

  try:
    if is_lz4:
      decompress_lz4(resolved_input, tmp_output)
    elif is_zstd:
      decompress_zstd(resolved_input, tmp_output)
    elif is_so:
      shutil.copyfile(resolved_input, tmp_output)
      shutil.copymode(resolved_input, tmp_output)
    else:
      _, ext = os.path.splitext(resolved_input)
      raise ValueError(
          f"Unsupported prebuilt libcobalt file format '{ext}' for file "
          f"'{input_path}'. Supported formats are uncompressed .so, .lz4, "
          'and .zstd/.zst.')

    os.replace(tmp_output, output_path)
  finally:
    if os.path.exists(tmp_output):
      try:
        os.remove(tmp_output)
      except OSError:
        pass

  if unstripped_output:
    os.makedirs(
        os.path.dirname(os.path.abspath(unstripped_output)), exist_ok=True)
    unstripped_tmp = unstripped_output + '.tmp'
    try:
      shutil.copyfile(output_path, unstripped_tmp)
      shutil.copymode(output_path, unstripped_tmp)
      os.replace(unstripped_tmp, unstripped_output)
    finally:
      if os.path.exists(unstripped_tmp):
        try:
          os.remove(unstripped_tmp)
        except OSError:
          pass


def main():
  parser = argparse.ArgumentParser(
      description='Copy or decompress prebuilt libcobalt library.')
  parser.add_argument(
      '--input',
      required=True,
      help='Path to the prebuilt libcobalt file (.so, .lz4, .zstd, .zst).')
  parser.add_argument(
      '--output',
      required=True,
      help='Destination output path for libcobalt.so.')
  parser.add_argument(
      '--repo-root',
      default=None,
      help='Root directory of the repository for relative path resolution.')
  parser.add_argument(
      '--build-dir',
      default=None,
      help='Root build directory for relative path resolution.')
  parser.add_argument(
      '--unstripped-output',
      default=None,
      help='Optional destination output path for unstripped libcobalt.so.')

  args = parser.parse_args()

  try:
    copy_or_decompress_prebuilt(
        input_path=args.input,
        output_path=args.output,
        repo_root=args.repo_root,
        build_dir=args.build_dir,
        unstripped_output=args.unstripped_output)
  except Exception as e:  # pylint: disable=broad-except
    print(f'Error: {e}', file=sys.stderr)
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
