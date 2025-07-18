#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is a wrapper around the GN binary that is pulled from Google
Cloud Storage when you sync Chrome. The binaries go into platform-specific
subdirectories in the source tree.

This script makes there be one place for forwarding to the correct platform's
binary. It will also automatically try to find the gn binary when run inside
the chrome source tree, so users can just type "gn" on the command line
(normally depot_tools is on the path)."""

from __future__ import print_function

import gclient_paths
import os
import subprocess
import sys


def PruneVirtualEnv():
  # Set by VirtualEnv, no need to keep it.
  os.environ.pop('VIRTUAL_ENV', None)

  # Set by VPython, if scripts want it back they have to set it explicitly.
  os.environ.pop('PYTHONNOUSERSITE', None)

  # Look for "activate_this.py" in this path, which is installed by VirtualEnv.
  # This mechanism is used by vpython as well to sanitize VirtualEnvs from
  # $PATH.
  os.environ['PATH'] = os.pathsep.join([
    p for p in os.environ.get('PATH', '').split(os.pathsep)
    if not os.path.isfile(os.path.join(p, 'activate_this.py'))
  ])


def main(args):
  # Prune all evidence of VPython/VirtualEnv out of the environment. This means
  # that we 'unwrap' vpython VirtualEnv path/env manipulation. Invocations of
  # `python` from GN should never inherit the gn.py's own VirtualEnv. This also
  # helps to ensure that generated ninja files do not reference python.exe from
  # the VirtualEnv generated from depot_tools' own .vpython file (or lack
  # thereof), but instead reference the default python from the PATH.
  PruneVirtualEnv()

  # Try in primary solution location first, with the gn binary having been
  # downloaded by cipd in the projects DEPS.
  primary_solution_path = gclient_paths.GetPrimarySolutionPath()
  if primary_solution_path:
    gn_path = os.path.join(primary_solution_path, 'third_party',
                           'gn', 'gn' + gclient_paths.GetExeSuffix())
    if os.path.exists(gn_path):
      return subprocess.call([gn_path] + args[1:])

  # Otherwise try the old .sha1 and download_from_google_storage locations
  # inside of buildtools.
  bin_path = gclient_paths.GetBuildtoolsPlatformBinaryPath()
  if not bin_path:
    print('gn.py: Could not find checkout in any parent of the current path.\n'
          'This must be run inside a checkout.', file=sys.stderr)
    return 1
  gn_path = os.path.join(bin_path, 'gn' + gclient_paths.GetExeSuffix())
  if not os.path.exists(gn_path):
    print(
        'gn.py: Could not find gn executable at: %s' % gn_path, file=sys.stderr)
    return 2
  return subprocess.call([gn_path] + args[1:])


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(1)
