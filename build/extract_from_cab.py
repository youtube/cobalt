#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts a single file from a CAB archive."""

import os
import subprocess
import sys
import tempfile

lock_file = os.path.join(tempfile.gettempdir(), 'expand.lock')


def acquire_lock():
  while True:
    try:
      fd = os.open(lock_file, os.O_CREAT | os.O_EXCL | os.O_RDWR)
      return fd
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise
      print 'Cab extraction could not get exclusive lock. Retrying in 100ms...'
      time.sleep(0.1)


def release_lock(fd):
  os.close(fd)
  os.unlink(lock_file)


def main():
  if len(sys.argv) != 4:
    print 'Usage: extract_from_cab.py cab_path archived_file output_dir'
    return 1

  [cab_path, archived_file, output_dir] = sys.argv[1:]

  lock_fd = acquire_lock()
  try:
    # Invoke the Windows expand utility to extract the file.
    level = subprocess.call(
        ['expand', cab_path, '-F:' + archived_file, output_dir])
    if level != 0:
      return level
  finally:
    release_lock(lock_fd)

  # The expand utility preserves the modification date and time of the archived
  # file. Touch the extracted file. This helps build systems that compare the
  # modification times of input and output files to determine whether to do an
  # action.
  os.utime(os.path.join(output_dir, archived_file), None)
  return 0


if __name__ == '__main__':
  sys.exit(main())
