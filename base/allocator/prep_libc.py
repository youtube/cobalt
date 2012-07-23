#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script takes libcmt.lib for VS2005/08/10 and removes the allocation
# related functions from it.
#
# Usage: prep_libc.py <VCInstallDir> <OutputDir>
#
# VCInstallDir is the path where VC is installed, something like:
#    C:\Program Files\Microsoft Visual Studio 8\VC\
#
# OutputDir is the directory where the modified libcmt file should be stored.

import os
import shutil
import subprocess
import sys

def run(command, filter=None):
  """Run |command|, removing any lines that match |filter|. The filter is
  to remove the echoing of input filename that 'lib' does."""
  popen = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  for line in out.splitlines():
    if filter and line.strip() != filter:
      print line
  return popen.returncode

def main():
  vs_install_dir = sys.argv[1]
  outdir = sys.argv[2]
  output_lib = os.path.join(outdir, 'libcmt.lib')
  shutil.copyfile(os.path.join(vs_install_dir, 'libcmt.lib'), output_lib)
  shutil.copyfile(os.path.join(vs_install_dir, 'libcmt.pdb'),
                  os.path.join(outdir, 'libcmt.pdb'))
  vspaths = [
    'build\\intel\\mt_obj\\',
    'f:\\dd\\vctools\\crt_bld\\SELF_X86\\crt\\src\\build\\INTEL\\mt_obj\\'
  ]
  objfiles = ['malloc', 'free', 'realloc', 'new', 'delete', 'new2', 'delete2',
              'align', 'msize', 'heapinit', 'expand', 'heapchk', 'heapwalk',
              'heapmin', 'sbheap', 'calloc', 'recalloc', 'calloc_impl',
              'new_mode', 'newopnt']
  for obj in objfiles:
    for vspath in vspaths:
      cmd = ('lib /nologo /ignore:4006,4014,4221 /remove:%s%s.obj %s' %
             (vspath, obj, output_lib))
      run(cmd, obj + '.obj')

if __name__ == "__main__":
  sys.exit(main())
