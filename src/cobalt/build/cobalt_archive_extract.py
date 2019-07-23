#!/usr/bin/env python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

"""Tools for extracting a Cobalt Archive.

This no-dependency tool will extract a Cobalt Archive across all platforms.

This is slightly complicated because of issues on Windows where the poor
support for pathnames longer than 255 characters is an issue.
"""


import argparse
import logging
import os
import shutil
import subprocess
import sys
import zipfile


################################################################################
#                                  API                                         #
################################################################################


def ExtractTo(archive_zip_path, output_dir, outstream=None):
  return _ExtractTo(archive_zip_path=archive_zip_path,
                    output_dir=output_dir,
                    outstream=outstream)


def ToWinUncPath(dos_path, encoding=None):
  """Returns a windows UNC path which enables long path names in win32 apis."""
  return _ToWinUncPath(dos_path, encoding)


################################################################################
#                                 IMPL                                         #
################################################################################


_OUT_ARCHIVE_ROOT = '__cobalt_archive'
_OUT_FINALIZE_DECOMPRESSION_PATH = '%s/%s' % (_OUT_ARCHIVE_ROOT,
                                              'finalize_decompression')
_OUT_DECOMP_PY = '%s/%s' % (_OUT_FINALIZE_DECOMPRESSION_PATH,
                            'decompress.py')
_OUT_DECOMP_JSON = '%s/%s' % (_OUT_FINALIZE_DECOMPRESSION_PATH,
                              'decompress.json')

_IS_WINDOWS = sys.platform in ['win32', 'cygwin']


def _ToWinUncPath(dos_path, encoding=None):
  """Windows supports long file names when using a UNC path."""
  assert _IS_WINDOWS
  do_convert = (not isinstance(dos_path, unicode) and encoding is not None)
  if do_convert:
    dos_path = dos_path.decode(encoding)
  path = os.path.abspath(dos_path)
  if path.startswith(u'\\\\'):
    return u'\\\\?\\UNC\\' + path[2:]
  return u'\\\\?\\' + path


def _GetZipFileClass():
  """Get the ZipFile class for the platform"""
  if not _IS_WINDOWS:
    return zipfile.ZipFile
  else:
    class ZipFileLongPaths(zipfile.ZipFile):
      """Handles extracting to paths longer than 255 characters."""

      def _extract_member(self, member, targetpath, pwd):
        targetpath = _ToWinUncPath(targetpath)
        return zipfile.ZipFile._extract_member(self, member, targetpath, pwd)
    return ZipFileLongPaths


def _UnzipFiles(input_zip_path, output_dir, outstream):
  """Returns True if all files were extracted, else False."""
  if outstream is None:
    outstream = sys.stdout
  all_ok = True
  zf_class = _GetZipFileClass()
  with zf_class(input_zip_path, 'r', allowZip64=True) as zf:
    for zinfo in zf.infolist():
      try:
        logging.debug('Extracting: %s -> %s', zinfo.filename, output_dir)
        zf.extract(zinfo, path=output_dir)
      except Exception as err:  # pylint: disable=broad-except
        msg = 'Exception happend during bundle extraction: ' + str(err) + '\n'
        outstream.write(msg)
        all_ok = False
  return all_ok


def _ExtractTo(archive_zip_path, output_dir, outstream=None):
  """Returns True if all files were extracted, False otherwise."""
  outstream = outstream if outstream else sys.stdout
  assert os.path.exists(archive_zip_path), 'No archive at %s' % archive_zip_path
  logging.info('UNZIPPING %s -> %s', archive_zip_path, output_dir)
  ok = _UnzipFiles(archive_zip_path, output_dir, outstream)
  # Now that all files have been extracted, execute the final decompress
  # step.
  decomp_py = os.path.abspath(os.path.join(output_dir, _OUT_DECOMP_PY))
  assert(os.path.isfile(decomp_py)), decomp_py
  cmd_str = 'python ' + decomp_py
  outstream.write('Executing: %s\n' % cmd_str)
  rc = subprocess.call(cmd_str, shell=True, stdout=outstream,
                       stderr=outstream)
  ok = ok & (rc == 0)
  return ok


def _CreateArgumentParser():
  """Creates a parser that will print the full help on failure to parse."""
  parser = argparse.ArgumentParser()
  parser.add_argument('archive_path', help='Archive to extract.')
  parser.add_argument('output_path', help='Output path to extract the archive.')
  parser.add_argument('--delete', action='store_true',
                      help='Deletes output_path if it exists.')
  parser.add_argument('--verbose', action='store_true')
  return parser


def main():
  parser = _CreateArgumentParser()
  # To make this future compatible parse_known_args() is used and any unknown
  # args will generate a warning rather than be a fatal event. This allows
  # any flags used in the future to be used on this tool.
  args, unknown_args = parser.parse_known_args()
  logging_lvl = logging.INFO
  if args.verbose:
    logging_lvl = logging.DEBUG
  fmt = '[%(filename)s:%(lineno)s:%(levelname)s] %(message)s'
  logging.basicConfig(format=fmt, level=logging_lvl)
  if unknown_args:
    logging.warning('Unknown (ignored) args: %s', unknown_args)
  if args.delete:
    logging.info('Removing previous folder at %s', args.output_path)
    shutil.rmtree(args.output_path, ignore_errors=True)
  all_ok = ExtractTo(args.archive_path, args.output_path)
  if all_ok:
    sys.exit(0)
  else:
    logging.critical('Errors happened.')
    sys.exit(1)


if __name__ == '__main__':
  main()

