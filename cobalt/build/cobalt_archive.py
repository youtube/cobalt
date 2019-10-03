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


"""Tools for creating and extracting a Cobalt Archive."""

import argparse
import fnmatch
import hashlib
import json
import logging
import os
import random
import stat
import sys
import time
import zipfile

import _env  # pylint: disable=relative-import,unused-import
from cobalt.build import cobalt_archive_extract
import starboard.build.filelist as filelist
from starboard.tools.app_launcher_packager import CopyAppLauncherTools
from starboard.tools.build import GetPlatformConfig
from starboard.tools.config import GetAll as GetAllConfigs
import starboard.tools.paths as paths
from starboard.tools.platform import GetAll as GetAllPlatforms
import starboard.tools.port_symlink as port_symlink
from starboard.tools.util import SetupDefaultLoggingConfig


################################################################################
#                                  API                                         #
################################################################################


def MakeCobaltArchiveFromFileList(output_archive_path,
                                  input_file_list,  # class FileList
                                  platform_name,
                                  platform_sdk_version,
                                  config,
                                  additional_buildinfo_dict=None):
  if additional_buildinfo_dict is None:
    additional_buildinfo_dict = {}
  archive = CobaltArchive(archive_zip_path=output_archive_path)
  archive.MakeArchive(platform_name=platform_name,
                      platform_sdk_version=platform_sdk_version,
                      config=config,
                      file_list=input_file_list,
                      additional_buildinfo_dict=additional_buildinfo_dict)


def MakeCobaltArchiveFromSource(output_archive_path,
                                platform_name,
                                config,
                                platform_sdk_version,
                                additional_buildinfo_dict=None,
                                include_black_box_tests=False):
  """Returns None, failure is signaled via exception."""
  if additional_buildinfo_dict is None:
    additional_buildinfo_dict = {}
  _MakeCobaltArchiveFromSource(
      output_archive_path=output_archive_path,
      platform_name=platform_name,
      config=config,
      platform_sdk_version=platform_sdk_version,
      additional_buildinfo_dict=additional_buildinfo_dict,
      include_black_box_tests=include_black_box_tests)


def ExtractCobaltArchive(input_zip_path,
                         output_directory_path,
                         outstream=None):
  """Returns True if the extract operation was successfull."""
  archive = CobaltArchive(archive_zip_path=input_zip_path)
  return archive.ExtractTo(output_dir=output_directory_path,
                           outstream=outstream)


def ReadCobaltArchiveInfo(input_zip_path):
  archive = CobaltArchive(archive_zip_path=input_zip_path)
  return archive.ReadMetaData()


################################################################################
#                                 IMPL                                         #
################################################################################


# Source resource paths.
_SELF_DIR = os.path.abspath(os.path.dirname(__file__))
_SRC_CONTENT_PATH = os.path.join(_SELF_DIR, 'cobalt_archive_content')


# Relative paths from the resulting archive root. The path seperator
# is normalized to '/'.
_OUT_ARCHIVE_ROOT = '__cobalt_archive'
_OUT_FINALIZE_DECOMPRESSION_PATH = '%s/%s' % (_OUT_ARCHIVE_ROOT,
                                              'finalize_decompression')
_OUT_METADATA_PATH = '%s/%s' % (_OUT_ARCHIVE_ROOT, 'metadata.json')
_OUT_DECOMP_JSON = '%s/%s' % (_OUT_FINALIZE_DECOMPRESSION_PATH,
                              'decompress.json')


class CobaltArchive(object):
  """CobaltArchive is a utility generating archives."""

  def __init__(self, archive_zip_path):
    self.archive_zip_path = archive_zip_path

  def ExtractTo(self, output_dir, outstream=None):
    """Returns True if all files were extracted, False otherwise."""
    return cobalt_archive_extract.ExtractTo(
        self.archive_zip_path, output_dir, outstream)

  def ReadMetaData(self):
    json_str = self.ReadFile(_OUT_METADATA_PATH)
    return json.loads(json_str)

  def ReadFile(self, file_path):
    with zipfile.ZipFile(self.archive_zip_path, 'r', allowZip64=True) as zf:
      return zf.read(file_path)

  def MakeArchive(self,
                  platform_name,
                  platform_sdk_version,
                  config,
                  file_list,  # class FileList
                  additional_buildinfo_dict=None):
    """Creates an archive for the given platform and config."""
    logging.info('Making cobalt archive...')
    is_windows = port_symlink.IsWindows()
    if additional_buildinfo_dict is None:
      additional_buildinfo_dict = {}
    if config not in GetAllConfigs():
      raise ValueError('Expected %s to be one of %s'
                       % (config, GetAllConfigs()))
    additional_buildinfo_dict = dict(additional_buildinfo_dict)  # Copy
    build_info_str = _GenerateBuildInfoStr(
        platform_name=platform_name,
        platform_sdk_version=platform_sdk_version,
        config=config,
        additional_buildinfo_dict=additional_buildinfo_dict)
    with zipfile.ZipFile(self.archive_zip_path, mode='w',
                         compression=zipfile.ZIP_DEFLATED,
                         allowZip64=True) as zf:
      # Copy the cobalt_archive_content directory into the root of the archive.
      content_file_list = filelist.FileList()
      content_file_list.AddAllFilesInPath(root_dir=_SRC_CONTENT_PATH,
                                          sub_path=_SRC_CONTENT_PATH)
      for file_path, archive_path in content_file_list.file_list:
        # Skip the fake metadata.json file because the real one
        # is a generated in it's place.
        if os.path.basename(file_path) == 'metadata.json':
          continue
        zf.write(file_path, arcname=archive_path)
      # Write out the metadata.
      zf.writestr(_OUT_METADATA_PATH, build_info_str)
      if file_list.file_list:
        logging.info('  Compressing %d files', len(file_list.file_list))

      executable_files = []
      n_file_list = len(file_list.file_list)
      progress_set = set()
      for i in range(n_file_list):
        # Logging every 5% increment during compression step.
        prog = int((float(i)/n_file_list) * 100)
        if prog not in progress_set:
          progress_set.add(prog)
          logging.info('  Compressed %d%%...', prog)
        file_path, archive_path = file_list.file_list[i]
        if not is_windows:
          perms = _GetFilePermissions(file_path)
          if (stat.S_IXUSR) & perms:
            executable_files.append(archive_path)
        # TODO: Use and implement _FoldIdenticalFiles() to reduce
        # duplicate files. This will help platforms like nxswitch which include
        # a lot of duplicate files for the sdk.
        try:
          zf.write(file_path, arcname=archive_path)
        except WindowsError:  # pylint: disable=undefined-variable
          # Happens for long file path names.
          zf.write(cobalt_archive_extract.ToWinUncPath(file_path),
                   arcname=archive_path)

      if file_list.symlink_dir_list:
        logging.info('  Compressing %d symlinks',
                     len(file_list.symlink_dir_list))
      # Generate the decompress.json file used by decompress.py.
      # Removes the first element which is the root directory, which is not
      # important for symlink creation.
      symlink_dir_list = [l[1:] for l in file_list.symlink_dir_list]
      # Replace '\\' with '/'
      symlink_dir_list = [_ToUnixPaths(l) for l in symlink_dir_list]
      decompress_json_str = _JsonDumpPrettyPrint({
          'symlink_dir': symlink_dir_list,
          'symlink_dir_doc': '[link_dir_path, target_dir_path]',
          'executable_files': executable_files,
      })
      zf.writestr(_OUT_DECOMP_JSON, decompress_json_str)
      logging.info('Done...')


def _ToUnixPaths(path_list):
  out = []
  for p in path_list:
    out.append(p.replace('\\', '/'))
  return out


def _GetFilePermissions(path):
  return stat.S_IMODE(os.stat(path).st_mode)


def _JsonDumpPrettyPrint(data):
  return json.dumps(data, sort_keys=True, indent=4, separators=(',', ': '))


def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


def _FindPossibleDeployPaths(build_root):
  """Searches for folders that are likely required for archiving."""
  out = []
  # Ultimately, this function should not be needed as each platform should
  # implement GetDeployPathPatterns(). This is stop-gap for platforms that do
  # not have GetDeployPathPatterns() implemented yet.
  root_paths = os.listdir(build_root)
  for p in root_paths:
    if p in ('gen', 'gypfiles', 'gyp-win-tool', 'obj', 'obj.host'):
      continue
    if p.endswith('.pdb'):
      continue  # Skip pdb files for size (only applies to Windows).
    p = os.path.join(build_root, p)
    if os.path.isfile(p):
      out.append(p)
      continue
    if port_symlink.IsSymLink(p):
      continue
    out.append(os.path.normpath(p))
  return out


def _PathMatchesPatterns(file_path, patterns):
  for p in patterns:
    if fnmatch.fnmatch(file_path, p):
      logging.debug('pattern %s matched %s', p, file_path)
      return True
  logging.debug('Skipping %s', file_path)
  return False


def _GetDeployPaths(platform_name, config):
  """Returns a list of paths that should be included in the archive."""
  try:
    gyp_config = GetPlatformConfig(platform_name)
    patterns = gyp_config.GetDeployPathPatterns()
    logging.info('Found platform include patterns: [%s]', ', '.join(patterns))
    out_directory = paths.BuildOutputDirectory(platform_name, config)
    out_paths = []
    for root, _, files in port_symlink.OsWalk(out_directory):
      for f in files:
        full_path = os.path.join(root, f)
        file_path = os.path.relpath(full_path, out_directory)
        if _PathMatchesPatterns(file_path, patterns):
          out_paths.append(file_path)
    return out_paths
  except NotImplementedError:  # Abstract class throws NotImplementedError.
    logging.warning('** AUTO INCLUDE: ** Specific deploy paths were not found '
                    'so including known possible deploy paths from the '
                    'platform out directory.')
    build_root = paths.BuildOutputDirectory(platform_name, config)
    deploy_paths = _FindPossibleDeployPaths(build_root)
    return deploy_paths


def _MakeCobaltArchiveFromSource(output_archive_path,
                                 platform_name,
                                 config,
                                 platform_sdk_version,
                                 additional_buildinfo_dict,
                                 include_black_box_tests):
  """Finds necessary files and makes an archive."""
  _MakeDirs(os.path.dirname(output_archive_path))
  out_directory = paths.BuildOutputDirectory(platform_name, config)
  root_dir = os.path.abspath(
      os.path.normpath(os.path.join(out_directory, '..', '..')))
  flist = filelist.FileList()
  inc_paths = _GetDeployPaths(platform_name, config)
  logging.info('Adding binary files to bundle...')
  for path in inc_paths:
    path = os.path.join(out_directory, path)
    if not os.path.exists(path):
      logging.info('Skipping deploy path %s because it does not exist.',
                   path)
      continue
    logging.info('  adding %s', os.path.abspath(path))
    flist.AddAllFilesInPath(root_dir=root_dir, sub_path=path)
  logging.info('...done')
  launcher_tools_path = os.path.join(
      os.path.dirname(output_archive_path),
      '____app_launcher')
  if os.path.exists(launcher_tools_path):
    port_symlink.Rmtree(launcher_tools_path)
  logging.info('Adding app_launcher_files to bundle in %s',
               os.path.abspath(launcher_tools_path))

  try:
    CopyAppLauncherTools(repo_root=paths.REPOSITORY_ROOT,
                         dest_root=launcher_tools_path,
                         additional_glob_patterns=[],
                         include_black_box_tests=include_black_box_tests)
    flist.AddAllFilesInPath(root_dir=launcher_tools_path,
                            sub_path=launcher_tools_path)
    logging.info('...done')

    MakeCobaltArchiveFromFileList(
        output_archive_path,
        input_file_list=flist,
        platform_name=platform_name,
        platform_sdk_version=platform_sdk_version,
        config=config,
        additional_buildinfo_dict=additional_buildinfo_dict)
    logging.info('...done')
  finally:
    port_symlink.Rmtree(launcher_tools_path)


def _FoldIdenticalFiles(file_path_list):
  """Takes input files and determines which are md5 identical and folds them.

  TODO: Implement into Cobalt Archive.

  Example:
    files, copy_list = _FoldIdenticalFiles(['in0/test.txt', 'in1/test.txt'])
    Output:
      files => ['in0/test.txt']
      copy_list => ['in0/test.txt', 'in1/test.txt']

  Args:
    file_path_list: A list of files that will be processed.

  Returns:
    A 2-tuple (files, copy_list) where files is a list of physical files and
    copy_list is the list of files that are identical.
  """
  # Remove duplicates.
  file_path_list = list(set(file_path_list))
  file_path_list.sort()
  def Md5File(fpath):
    hash_md5 = hashlib.md5()
    with open(fpath, 'rb') as f:
      for chunk in iter(lambda: f.read(4096), b''):
        hash_md5.update(chunk)
    return hash_md5.hexdigest()
  # Output
  phy_file_list = []
  copy_list = []
  # Temp data structure.
  file_map = {}
  for file_path in file_path_list:
    name = os.path.basename(file_path)
    fsize = os.stat(file_path).st_size
    entry = (name, fsize)
    files = file_map.get(entry, [])
    files.append(file_path)
    file_map[entry] = files
  for (fname, fsize), path_list in file_map.iteritems():  # pylint: disable=unused-variable
    assert path_list
    phy_file_list.append(path_list[0])
    if len(path_list) == 1:
      continue
    else:
      md5_dict = {Md5File(path_list[0]): path_list[0]}
      for tail_file in path_list[1:]:
        new_md5 = Md5File(tail_file)
        matching_file = md5_dict.get(new_md5, None)
        if matching_file is not None:
          # Match found.
          copy_list.append((matching_file, tail_file))
        else:
          phy_file_list.append(tail_file)
          md5_dict[new_md5] = tail_file
  return phy_file_list, copy_list


def _GenerateBuildInfoStr(platform_name, platform_sdk_version,
                          config, additional_buildinfo_dict):
  """Generates a build info string (for the metadata file)."""
  build_info = dict(additional_buildinfo_dict)  # Copy dict.
  build_info['archive_time_RFC_2822'] = (
      time.strftime('%a, %d %b %Y %H:%M:%S +0000', time.gmtime()))
  build_info['archive_time_local'] = time.asctime()
  build_info['platform'] = platform_name
  build_info['config'] = config
  build_info['sdk_version'] = platform_sdk_version
  # Can be used by clients for caching reasons.
  build_info['nonce'] = random.randint(0, 0xffffffffffffffff)
  build_info_str = _JsonDumpPrettyPrint(build_info)
  return build_info_str


################################################################################
#                                CMD LINE                                      #
################################################################################


def _MakeCobaltPlatformArchive(platform, config, output_zip,
                               include_black_box_tests):
  """Makes a Cobalt Archive, prompting for missing platform/config."""
  if not platform:
    platform = raw_input('platform: ')
  if platform not in GetAllPlatforms():
    raise ValueError('Platform "%s" not recognized, expected one of: \n%s'
                     % (platform, GetAllPlatforms()))
  if not config:
    config = raw_input('config: ')
  if not output_zip:
    output_zip = os.path.normpath(raw_input('output_zip: '))
  if not output_zip.endswith('.zip'):
    output_zip += '.zip'
  start_time = time.time()
  MakeCobaltArchiveFromSource(
      output_zip,
      platform,
      config,
      platform_sdk_version='TEST',
      additional_buildinfo_dict=None,
      include_black_box_tests=include_black_box_tests)
  time_delta = time.time() - start_time
  if not os.path.isfile(output_zip):
    raise ValueError('Expected zip file at ' + output_zip)
  logging.info('\nGenerated: %s in %d seconds', output_zip, int(time_delta))


# Returns True/False
def _DecompressArchive(in_zip, out_path):
  if not in_zip:
    in_zip = raw_input('cobalt archive path: ')
  if not out_path:
    out_path = raw_input('output path: ')
  return ExtractCobaltArchive(input_zip_path=in_zip,
                              output_directory_path=out_path)


def _CreateArgumentParser():
  """Creates a parser that will print the full help on failure to parse."""

  class MyParser(argparse.ArgumentParser):

    def error(self, message):
      sys.stderr.write('error: %s\n' % message)
      self.print_help()
      sys.exit(2)
  help_msg = (
      'Example 1:\n'
      '  python cobalt_archive.py --create --platform nxswitch'
      ' --config devel --out_path <OUT_ZIP>\n\n'
      'Example 2:\n'
      '  python cobalt_archive.py --extract --in_path <ARCHIVE_PATH.ZIP>'
      ' --out_path <OUT_DIR>')
  # Enables new lines in the description and epilog.
  formatter_class = argparse.RawDescriptionHelpFormatter
  parser = MyParser(epilog=help_msg, formatter_class=formatter_class)
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-c',
      '--create',
      help='Creates an archive from source directory, optional arguments'
           ' include --platform --config and --out_path',
      action='store_true')
  group.add_argument(
      '-x',
      '--extract',
      help='Extract archive from IN_PATH to OUT_PATH, optional arguments '
           'include --in_path and --out_path',
      action='store_true')
  parser.add_argument('--platform', type=str,
                      help='Optional, used for --create',
                      default=None)
  parser.add_argument('--config', type=str,
                      help='Optional, used for --create',
                      choices=GetAllConfigs(),
                      default=None)
  parser.add_argument('--out_path', type=str,
                      help='Optional, used for --create and --decompress',
                      default=None)
  parser.add_argument('--in_path', type=str,
                      help='Optional, used for decompress',
                      default=None)
  parser.add_argument('--include_black_box_tests',
                      help='Optional, used for --create to add blackbox tests',
                      action='store_true')
  return parser


def main():
  SetupDefaultLoggingConfig()
  parser = _CreateArgumentParser()
  args, unknown_args = parser.parse_known_args()
  if unknown_args:
    logging.warning('Unknown (ignored) args: %s', unknown_args)
  if args.create:
    _MakeCobaltPlatformArchive(
        platform=args.platform,
        config=args.config,
        output_zip=os.path.normpath(args.out_path),
        include_black_box_tests=args.include_black_box_tests)
    sys.exit(0)
  elif args.extract:
    ok = _DecompressArchive(args.in_path, args.out_path)
    rc = 0 if ok else 1
    sys.exit(rc)
  else:
    parser.print_help()


if __name__ == '__main__':
  main()
