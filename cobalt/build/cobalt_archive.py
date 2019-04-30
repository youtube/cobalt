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


################################################################################
#                                  API                                         #
################################################################################


def MakeCobaltArchiveFromFileList(output_archive_path,
                                  input_file_list,  # class FileList
                                  platform_name,
                                  platform_sdk_version,
                                  config,
                                  additional_buildinfo_dict={}):
  additional_buildinfo_dict = dict(additional_buildinfo_dict)
  b = Bundler(archive_zip_path=output_archive_path)
  b.MakeArchive(platform_name=platform_name,
                platform_sdk_version=platform_sdk_version,
                config=config,
                file_list=input_file_list,
                additional_buildinfo_dict=additional_buildinfo_dict)


def MakeCobaltArchiveFromSource(output_archive_path,
                                platform_name,
                                config,
                                platform_sdk_version,
                                additional_buildinfo_dict={},
                                include_black_box_tests=False):
  """ Returns None, failure is signaled via exception. """
  additional_buildinfo_dict = dict(additional_buildinfo_dict)
  _MakeCobaltArchiveFromSource(
      output_archive_path=output_archive_path,
      platform_name=platform_name,
      config=config,
      platform_sdk_version=platform_sdk_version,
      additional_buildinfo_dict=additional_buildinfo_dict,
      include_black_box_tests=include_black_box_tests)


def ExtractCobaltArchive(input_zip_path, output_directory_path, outstream=None):
  """Returns True if the extract operation was successfull."""
  b = Bundler(archive_zip_path=input_zip_path)
  return b.ExtractTo(output_dir=output_directory_path, outstream=outstream)


def ReadCobaltArchiveInfo(input_zip_path):
  b = Bundler(archive_zip_path=input_zip_path)
  return b.ReadMetaData()


################################################################################
#                                 IMPL                                         #
################################################################################


import argparse
import hashlib
import json
import md5
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
import traceback
import zipfile

from sets import Set

import _env

from starboard.build.port_symlink \
    import IsSymLink, IsWindows, MakeSymLink, ReadSymLink, Rmtree, OsWalk

from starboard.tools.app_launcher_packager import CopyAppLauncherTools
from starboard.tools.paths import BuildOutputDirectory, REPOSITORY_ROOT

from starboard.build.filelist import \
    FileList, GetFileType, TempFileSystem, \
    TYPE_NONE, TYPE_SYMLINK_DIR, TYPE_DIRECTORY, TYPE_FILE

from starboard.build.port_symlink import Rmtree
from starboard.tools.build import GetPlatformConfig
from starboard.tools.config import GetAll as GetAllConfigs
from starboard.tools.platform import GetAll as GetAllPlatforms


# Source resource paths.
_SELF_DIR = os.path.abspath(os.path.dirname(__file__))
_SRC_CONTENT_PATH = os.path.join(_SELF_DIR, 'cobalt_archive_content')


# Relative paths from the resulting archive root. The path seperator
# is normalized to '/'.
_OUT_ARCHIVE_ROOT = '__cobalt_archive'
_OUT_FINALIZE_DECOMPRESSION_PATH = '%s/%s' % (_OUT_ARCHIVE_ROOT,
                                             'finalize_decompression')
_OUT_METADATA_PATH = '%s/%s' % (_OUT_ARCHIVE_ROOT, 'metadata.json')
_OUT_DECOMP_PY = '%s/%s' % (_OUT_FINALIZE_DECOMPRESSION_PATH,
                           'decompress.py')
_OUT_DECOMP_JSON = '%s/%s' % (_OUT_FINALIZE_DECOMPRESSION_PATH,
                             'decompress.json')


class Bundler:
  """Bundler is a utility for managing device bundles of codes. It is
  used for creating the zip file and also unzipping."""
  def __init__(self, archive_zip_path):
    self.archive_zip_path = archive_zip_path

  def ExtractTo(self, output_dir, outstream=None):
    """Returns True if all files were extracted, False otherwise."""
    outstream = outstream if outstream else sys.stdout
    assert(os.path.exists(self.archive_zip_path))
    print('UNZIPPING ' + self.archive_zip_path + ' -> ' + output_dir)
    ok = _ExtractFiles(self.archive_zip_path, output_dir, outstream)
    # Now that all files have been extracted, execute the final decompress
    # step.
    decomp_py = os.path.abspath(os.path.join(output_dir, _OUT_DECOMP_PY))
    assert(os.path.isfile(decomp_py)), decomp_py
    cmd_str = 'python ' + decomp_py
    outstream.write('Executing: %s\n' % cmd_str)
    rc = subprocess.call(cmd_str, shell=True, stdout=outstream,
                         stderr=outstream)
    ok &= rc is 0
    return ok

  def ReadMetaData(self):
    with zipfile.ZipFile(self.archive_zip_path, 'r', allowZip64=True) as zf:
      json_str = zf.read(_OUT_METADATA_PATH)
    return json.loads(json_str)

  def MakeArchive(self,
                  platform_name,
                  platform_sdk_version,
                  config,
                  file_list,  # class FileList
                  additional_buildinfo_dict={}):
    if not config in GetAllConfigs():
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
      content_file_list = FileList()
      content_file_list.AddAllFilesInPath(root_dir=_SRC_CONTENT_PATH,
                                          sub_dir=_SRC_CONTENT_PATH)
      for file_path, archive_path in content_file_list.file_list:
        # Skip the fake metadata.json file because the real one
        # is a generated in it's place.
        if os.path.basename(file_path) == 'metadata.json':
          continue
        zf.write(file_path, arcname=archive_path)
      # Write out the metadata.
      zf.writestr(_OUT_METADATA_PATH, build_info_str)
      if file_list.file_list:
        print('  Compressing ' + str(len(file_list.file_list)) + ' files')
      for file_path, archive_path in file_list.file_list:
        # TODO: Use and implement _FoldIdenticalFiles() to reduce duplicate
        # files. This will help platforms like nxswitch which include a lot
        # of duplicate files for the sdk.
        zf.write(file_path, arcname=archive_path)
      if file_list.symlink_dir_list:
        print('  Compressing ' + str(len(file_list.symlink_dir_list)) + ' symlinks')
      # Generate the decompress.json file used by decompress.py.
      # Removes the first element which is the root directory, which is not
      # important for symlink creation.
      symlink_dir_list = [l[1:] for l in file_list.symlink_dir_list]
      # Replace '\\' with '/'
      symlink_dir_list = [_ToUnixPaths(l) for l in symlink_dir_list]
      decompress_json_str = _JsonDumpPrettyPrint({
        'symlink_dir': symlink_dir_list,
        'symlink_dir_doc': '[link_dir_path, target_dir_path]'
      })
      zf.writestr(_OUT_DECOMP_JSON, decompress_json_str)


def _ToUnixPaths(path_list):
  out = []
  for p in path_list:
    out.append(p.replace('\\', '/'))
  return out


def _JsonDumpPrettyPrint(data):
  return json.dumps(data, sort_keys=True, indent=4, separators=(',', ': '))


def _CheckChildPathIsUnderParentPath(child_path, parent_path):
  parent_path = os.path.join(
      os.path.realpath(parent_path), '')
  f = os.path.realpath(child_path)
  common_prefix = os.path.commonprefix([child_path, parent_path])
  if common_prefix != parent_path:
    raise ValueError('Bundle files MUST be located under ' + \
                       parent_path + ', BadFile = ' + f)


def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


def _FindPossibleDeployDirs(build_root):
  out = []
  root_dirs = os.listdir(build_root)
  for d in root_dirs:
    if d in ('gen', 'gypfiles', 'obj', 'obj.host'):
      continue
    d = os.path.join(build_root, d)
    if os.path.isfile(d):
      continue
    if IsSymLink(d):
      continue
    out.append(os.path.normpath(d))
  return out


def _GetDeployDirs(platform_name, config):
  try:
    gyp_config = GetPlatformConfig(platform_name)
    return gyp_config.GetDeployDirs()
  except NotImplementedError:
    # TODO: Investigate why Logging.info(...) eats messages even when
    # the logging module is set to logging level INFO or DEBUG and when
    # this is fixed then replace the print functions in this file with logging.
    print('\n********************************************************\n'
          '%s: specific deploy directories not found, auto \n'
          'including possible deploy directories from build root.'
          '\n********************************************************\n'
          % __file__)
    build_root = BuildOutputDirectory(platform_name, config)
    return _FindPossibleDeployDirs(build_root)


def _MakeCobaltArchiveFromSource(output_archive_path,
                                 platform_name,
                                 config,
                                 platform_sdk_version,
                                 additional_buildinfo_dict,
                                 include_black_box_tests):
  _MakeDirs(os.path.dirname(output_archive_path))
  out_directory = BuildOutputDirectory(platform_name, config)
  root_dir = os.path.abspath(os.path.join(out_directory, '..', '..'))
  flist = FileList()
  inc_dirs = _GetDeployDirs(platform_name, config)
  print 'Adding binary files to bundle...'
  for path in inc_dirs:
    path = os.path.join(out_directory, path)
    if not os.path.exists(path):
      print 'Skipping deploy directory', path, 'because it does not exist.'
      continue
    print '  adding ' + os.path.abspath(path)
    flist.AddAllFilesInPath(root_dir=root_dir, sub_dir=path)
  print '...done'
  launcher_tools_path = os.path.join(
      os.path.dirname(output_archive_path),
      '____app_launcher')
  if os.path.exists(launcher_tools_path):
    Rmtree(launcher_tools_path)
  print ('Adding app_launcher_files to bundle in ' +
         os.path.abspath(launcher_tools_path))

  try:
    CopyAppLauncherTools(repo_root=REPOSITORY_ROOT,
                         dest_root=launcher_tools_path,
                         additional_glob_patterns=[],
                         include_black_box_tests=include_black_box_tests)
    flist.AddAllFilesInPath(root_dir=launcher_tools_path,
                            sub_dir=launcher_tools_path)
    print '...done'
    print 'Making cobalt archive...'
    MakeCobaltArchiveFromFileList(
        output_archive_path,
        input_file_list=flist,
        platform_name=platform_name,
        platform_sdk_version=platform_sdk_version,
        config=config,
        additional_buildinfo_dict=additional_buildinfo_dict)
    print '...done'
  finally:
    Rmtree(launcher_tools_path)


# TODO: Implement into bundler. This is unit tested at this time.
# Returns files_list, copy_list, where file_list is a list of physical
# files, and copy_list contains a 2-tuple of [physical_file, copy_location]
# that describes which files should be copied.
# Example:
#  files, copy_list = _FoldIdenticalFiles(['in0/test.txt', 'in1/test.txt'])
#  Output:
#    files => ['in0/test.txt']
#    copy_list => ['in0/test.txt', 'in1/test.txt']
def _FoldIdenticalFiles(file_path_list):
  # Remove duplicates.
  file_path_list = list(Set(file_path_list))
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
  for (fname, fsize), path_list in file_map.iteritems():
    assert(len(path_list))
    phy_file_list.append(path_list[0])
    if len(path_list) == 1:
      continue
    else:
      md5_dict = { Md5File(path_list[0]): path_list[0]  }
      for tail_file in path_list[1:]:
        new_md5 = Md5File(tail_file)
        matching_file = md5_dict.get(new_md5, None)
        if matching_file != None:
          # Match found.
          copy_list.append((matching_file, tail_file))
        else:
          phy_file_list.append(tail_file)
          md5_dict[new_md5] = tail_file
  return phy_file_list, copy_list


def _GenerateBuildInfoStr(platform_name, platform_sdk_version,
                          config, additional_buildinfo_dict):
  from time import gmtime, strftime
  build_info = dict(additional_buildinfo_dict)  # Copy dict.
  build_info['archive_time_RFC_2822'] = \
      strftime("%a, %d %b %Y %H:%M:%S +0000", gmtime())
  build_info['archive_time_local'] = time.asctime()
  build_info['platform'] = platform_name
  build_info['config'] = config
  build_info['sdk_version'] = platform_sdk_version
  # Can be used by clients for caching reasons.
  build_info['random_uint64'] = random.randint(0, 0xffffffffffffffff)
  build_info_str = _JsonDumpPrettyPrint(build_info)
  return build_info_str


def _AddFilesAndSymlinksToZip(open_zipfile, archive_file_list):
  sym_links = []
  for path, archive_name in archive_file_list:
    if IsSymLink(path):
      sym_links.append([path, archive_name])
    else:
      open_zipfile.write(path, arcname=archive_name)
  for path, archive_name in sym_links:
    zinfo = zipfile.ZipInfo(path)
    link_path = ReadSymLink(path)
    zinfo.filename = rel_path
    zinfo.comment = json.dumps({'symlink': link_path})
    open_zipfile.writestr(zinfo, rel_path, compress_type=zipfile.ZIP_DEFLATED)


# Returns True if all files were extracted, else False.
def _ExtractFiles(input_zip_path, output_dir, outstream):
  all_ok = True
  with zipfile.ZipFile(input_zip_path, 'r', allowZip64=True) as zf:
    for zinfo in zf.infolist():
      try:
        zf.extract(zinfo, path=output_dir)
      except Exception as err:
        msg = 'Exception happend during bundle extraction: ' + str(err) + '\n'
        outstream.write(msg)
        all_ok = False
  return all_ok


################################################################################
#                              UNIT TEST                                       #
################################################################################


def _UnitTestBundler_ExtractTo():
  flist = FileList()
  tf = TempFileSystem()
  tf.Clear()
  tf.Make()
  flist.AddSymLink(tf.root_in_tmp, tf.sym_dir)
  bundle_zip = os.path.join(tf.root_tmp, 'bundle.zip')
  b = Bundler(bundle_zip)
  b.MakeArchive(platform_name='fake',
                platform_sdk_version='fake_sdk',
                config='devel',
                file_list=flist)
  out_dir = os.path.join(tf.root_tmp, 'out')
  b.ExtractTo(out_dir)
  out_from_dir = os.path.join(out_dir, 'from_dir')
  out_from_dir_lnk = os.path.join(out_dir, 'from_dir_lnk')
  assert(GetFileType(out_from_dir) == TYPE_DIRECTORY)
  assert(GetFileType(out_from_dir_lnk) == TYPE_SYMLINK_DIR)
  resolved_from_link_path = os.path.join(out_dir, ReadSymLink(out_from_dir_lnk))
  assert(os.path.abspath(out_from_dir) ==
         os.path.abspath(resolved_from_link_path))


def _UnitTestBundler_MakesDeployInfo():
  flist = FileList()
  tf = TempFileSystem()
  tf.Clear()
  tf.Make()
  bundle_zip = os.path.join(tf.root_tmp, 'bundle.zip')
  b = Bundler(bundle_zip)
  b.MakeArchive(platform_name='fake',
                platform_sdk_version='fake_sdk',
                config='devel',
                file_list=flist)
  out_dir = os.path.join(tf.root_tmp, 'out')
  b.ExtractTo(out_dir)
  out_metadata_file = os.path.join(out_dir, _OUT_METADATA_PATH)
  assert(GetFileType(out_metadata_file) == TYPE_FILE)
  with open(out_metadata_file) as fd:
    text = fd.read()
    js = json.loads(text)
    assert(js)
    assert(js['sdk_version'] == 'fake_sdk')
    assert(js['platform'] == 'fake')
    assert(js['config'] == 'devel')


def _UnitTest_FoldIdenticalFiles():
  tf_root = TempFileSystem('bundler_fold')
  tf_root.Clear()
  tf1 = TempFileSystem(os.path.join('bundler_fold', '1'))
  tf2 = TempFileSystem(os.path.join('bundler_fold', '2'))
  tf1.Make()
  tf2.Make()
  flist = FileList()
  subdirs = [tf1.root_in_tmp, tf2.root_in_tmp]
  flist.AddAllFilesInPaths(tf_root.root_tmp, subdirs)
  flist.Print()
  identical_files = [tf1.test_txt, tf2.test_txt]
  physical_files, copy_files = _FoldIdenticalFiles(identical_files)
  assert(tf1.test_txt == physical_files[0])
  assert(tf1.test_txt in copy_files[0][0])
  assert(tf2.test_txt in copy_files[0][1])


def _UnitTest():
  tests = [
    ['UnitTestBundler_ExtractTo', _UnitTestBundler_ExtractTo],
    ['UnitTestBundler_MakesDeployInfo', _UnitTestBundler_MakesDeployInfo],
    ['UnitTest_FoldIdenticalFiles', _UnitTest_FoldIdenticalFiles],
  ]
  failed_tests = []
  for name, test in tests:
    try:
      print('\n' + name + ' started.')
      test()
      print(name + ' passed.')
    except Exception as err:
      failed_tests.append(name)
      traceback.print_exc()
      print('Error happened during test ' + name)

  if failed_tests:
    print '\n\nThe following tests failed: ' + ','.join(failed_tests)
    return 1
  else:
    return 0


################################################################################
#                                CMD LINE                                      #
################################################################################


def _MakeCobaltPlatformArchive(platform, config, output_zip,
                               include_black_box_tests):
  if not platform:
    platform = raw_input('platform: ')
  if not platform in GetAllPlatforms():
    raise ValueError('Platform "%s" not recognized, expected one of: \n%s'
                     % (platform, GetAllPlatforms()))
  if not config:
    config = raw_input('config: ')
  if not output_zip:
    output_zip = raw_input('output_zip: ')
  if not output_zip.endswith('.zip'):
    output_zip += '.zip'
  start_time = time.time()
  MakeCobaltArchiveFromSource(
      output_zip,
      platform,
      config,
      platform_sdk_version='TEST',
      additional_buildinfo_dict={},
      include_black_box_tests=include_black_box_tests)
  time_delta = time.time() - start_time
  if not os.path.isfile(output_zip):
    raise ValueError('Expected zip file at ' + output_zip)
  print '\nGenerated: %s in %d seconds' % (output_zip, int(time_delta))

# Returns True/False
def _DecompressArchive(in_zip, out_path):
  if not in_zip:
    in_zip = raw_input('cobalt archive path: ')
  if not out_path:
    out_path = raw_input('output path: ')
  return ExtractCobaltArchive(input_zip_path=in_zip,
                              output_directory_path=out_path)


def _main():
  # Create a private parser that will print the full help whenever the command
  # line fails to fully parse.
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
      '-u',
      '--unit_test',
      help='Run cobalt archive UnitTest',
      action='store_true')
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
  args = parser.parse_args()
  if args.unit_test:
    sys.exit(_UnitTest())
  elif args.create:
    _MakeCobaltPlatformArchive(
        platform=args.platform,
        config=args.config,
        output_zip=args.out_path,
        include_black_box_tests=args.include_black_box_tests)
    sys.exit(0)
  elif args.extract:
    ok = _DecompressArchive(args.in_path, args.out_path)
    rc = 0 if ok else 1
    sys.exit(rc)
  else:
    parser.print_help()


if __name__ == '__main__':
  _main()
