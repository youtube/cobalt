#!/usr/bin/env python

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import shutil
import sys
import tarfile

# A python script that unpacks the pumpkin tar archive by:
# 1. Extracting the tar into a temporary directory
# 2. Copying the file contents from the temporary directory to the destination
# directory
# 3. Removing the temporary directory
#
# We need to do the extraction indirectly because of:
# 1. how tarfile.extractall() works
# 2. how ninja determines dirty/stale objects
#
# tarfile.extractall() hits errors if the extracted file already exists.
# If we wanted to extract directly into the destination directory, we'd need to
# clear the directory first. However, removing and creating new directories
# within this script would change object timestamps without ninja's
# knowledge. This would cause ninja to always think the pumpkin test files are
# out of date, and thus this script would run each time there is a build
# request, even if there is no work necessary. This is all important because the
# CQ builds all targets, then triggers the same build again and asserts that
# it was a no-op. Without this indirect extraction, we'd fail the CQ every time.

def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <tar-file_path>'
  parser.add_option(
      '--dest-dir',
      action='store',
      metavar='DEST_DIR',
      help='Destination directory for extracted files.')
  options, args = parser.parse_args()
  if len(args) < 1 or not options.dest_dir:
      print(
          'Expected --dest-dir and the tar archive to unpack.',
          file=sys.stderr)
      print(str(args))
      sys.exit(1)

  tarArchive = args[0]
  outputFiles = args[1].split(',')
  pumpkinOutputDir = args[2];
  destDir = options.dest_dir
  tempDir = os.path.join(destDir, 'temp')

  # Remove full path from each output file so that they're relative to
  # pumpkinOutputDir.
  for i in range(0, len(outputFiles)):
    path = outputFiles[i]
    outputFiles[i] = path.replace(pumpkinOutputDir, "")

  # Extract tar into temporary directory.
  tar = tarfile.open(tarArchive)
  tar.extractall(path=tempDir)
  tar.close()

  # Copy file contents from tempDir to destDir.
  for file in outputFiles:
    source = tempDir + file
    destination = destDir + file
    shutil.copyfile(source, destination)

  # Remove temporary directory.
  shutil.rmtree(tempDir)

if __name__ == '__main__':
  main()
