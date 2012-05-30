#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# On Android we build unit test bundles as shared libraries.  To run
# tests, we launch a special "test runner" apk which loads the library
# then jumps into it.  Since java is required for many tests
# (e.g. PathUtils.java), a "pure native" test bundle is inadequate.
#
# This script, generate_native_test.py, is used to generate the source
# for an apk that wraps a unit test shared library bundle.  That
# allows us to have a single boiler-plate application be used across
# all unit test bundles.

import logging
import optparse
import os
import re
import shutil
import subprocess
import sys

# cmd_helper.py is under ../../build/android/
sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..',
   '..', 'build', 'android'))
import cmd_helper # pylint: disable=F0401


class NativeTestApkGenerator(object):
  """Generate a native test apk source tree.

  TODO(jrg): develop this more so the activity name is replaced as
  well.  That will allow multiple test runners to be installed at the
  same time.  (The complication is that it involves renaming a java
  class, which implies regeneration of a jni header, and on and on...)
  """

  # Files or directories we need to copy to create a complete apk test shell.
  _SOURCE_FILES = ['AndroidManifest.xml',
                   'native_test_apk.xml',
                   'res',  # res/values/strings.xml
                   'java', # .../ChromeNativeTestActivity.java
                   ]

  # Files in the destion directory that have a "replaceme" string
  # which should be replaced by the basename of the shared library.
  # Note we also update the filename if 'replaceme' is itself found in
  # the filename.
  _REPLACEME_FILES = ['AndroidManifest.xml',
                      'native_test_apk.xml',
                      'res/values/strings.xml']

  def __init__(self, native_library, jars, output_directory, target_abi):
    self._native_library = native_library
    self._jars = jars
    self._output_directory = output_directory
    self._target_abi = target_abi
    self._root_name = None
    if self._native_library:
      self._root_name = self._LibraryRoot()
    logging.warn('root name: %s' % self._root_name)


  def _LibraryRoot(self):
    """Return a root name for a shared library.

    The root name should be suitable for substitution in apk files
    like the manifest.  For example, blah/foo/libbase_unittests.so
    becomes base_unittests.
    """
    rootfinder = re.match('.?lib(.+).so',
                          os.path.basename(self._native_library))
    if rootfinder:
      return rootfinder.group(1)
    else:
      return None

  def _CopyTemplateFiles(self):
    """Copy files needed to build a new apk.

    TODO(jrg): add more smarts so we don't change file timestamps if
    the files don't change?
    """
    srcdir = os.path.dirname(os.path.realpath( __file__))
    if not os.path.exists(self._output_directory):
      os.makedirs(self._output_directory)
    for f in self._SOURCE_FILES:
      src = os.path.join(srcdir, f)
      dest = os.path.join(self._output_directory, f)
      if os.path.isfile(src):
        if os.path.exists(dest):
          os.remove(dest)
        logging.warn('%s --> %s' % (src, dest))
        shutil.copyfile(src, dest)
      else:  # directory
        if os.path.exists(dest):
          # One more sanity check since we're deleting a directory...
          if not '/out/' in dest:
            raise Exception('Unbelievable output directory; bailing for safety')
          shutil.rmtree(dest)
        logging.warn('%s --> %s' % (src, dest))
        shutil.copytree(src, dest)

  def _ReplaceStrings(self):
    """Replace 'replaceme' strings in generated files with a root libname.

    If we have no root libname (e.g. no shlib was specified), do nothing.
    """
    if not self._root_name:
      return
    logging.warn('Replacing "replaceme" with ' + self._root_name)
    for f in self._REPLACEME_FILES:
      dest = os.path.join(self._output_directory, f)
      contents = open(dest).read()
      contents = contents.replace('replaceme', self._root_name)
      dest = dest.replace('replaceme', self._root_name) # update the filename!
      open(dest, "w").write(contents)

  def _CopyLibraryAndJars(self):
    """Copy the shlib and jars into the apk source tree (if relevant)"""
    if self._native_library:
      destdir = os.path.join(self._output_directory, 'libs/' + self._target_abi)
      if not os.path.exists(destdir):
        os.makedirs(destdir)
      dest = os.path.join(destdir, os.path.basename(self._native_library))
      logging.warn('strip %s --> %s' % (self._native_library, dest))
      strip = os.environ['STRIP']
      cmd_helper.RunCmd(
          [strip, '--strip-unneeded', self._native_library, '-o', dest])
    if self._jars:
      destdir = os.path.join(self._output_directory, 'libs')
      if not os.path.exists(destdir):
        os.makedirs(destdir)
      for jar in self._jars:
        dest = os.path.join(destdir, os.path.basename(jar))
        logging.warn('%s --> %s' % (jar, dest))
        shutil.copyfile(jar, dest)

  def CreateBundle(self):
    """Create the apk bundle source and assemble components."""
    self._CopyTemplateFiles()
    self._ReplaceStrings()
    self._CopyLibraryAndJars()

  def Compile(self, ant_args):
    """Build the generated apk with ant.

    Args:
      ant_args: extra args to pass to ant
    """
    cmd = ['ant']
    if ant_args:
      cmd.append(ant_args)
    cmd.extend(['-buildfile',
                os.path.join(self._output_directory, 'native_test_apk.xml')])
    logging.warn(cmd)
    p = subprocess.Popen(cmd, stderr=subprocess.STDOUT)
    (stdout, _) = p.communicate()
    logging.warn(stdout)
    if p.returncode != 0:
      logging.error('Ant return code %d' % p.returncode)
      sys.exit(p.returncode)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--verbose',
                    help='Be verbose')
  parser.add_option('--native_library',
                    help='Full name of native shared library test bundle')
  parser.add_option('--jars',
                    help='Space separated list of jars to be included')
  parser.add_option('--output',
                    help='Output directory for generated files.')
  parser.add_option('--app_abi', default='armeabi',
                    help='ABI for native shared library')
  parser.add_option('--ant-compile', action='store_true',
                    help='If specified, build the generated apk with ant')
  parser.add_option('--ant-args',
                    help='extra args for ant')

  options, _ = parser.parse_args(argv)

  # It is not an error to specify no native library; the apk should
  # still be generated and build.  It will, however, print
  # NATIVE_LOADER_FAILED when run.
  if not options.output:
    raise Exception('No output directory specified for generated files')

  if options.verbose:
    logging.basicConfig(level=logging.DEBUG, format=' %(message)s')

  # Remove all quotes from the jars string
  jar_list = []
  if options.jars:
    jar_list = options.jars.replace('"', '').split()
  ntag = NativeTestApkGenerator(native_library=options.native_library,
                                jars=jar_list,
                                output_directory=options.output,
                                target_abi=options.app_abi)
  ntag.CreateBundle()
  if options.ant_compile:
    ntag.Compile(options.ant_args)

  logging.warn('COMPLETE.')

if __name__ == '__main__':
  sys.exit(main(sys.argv))
