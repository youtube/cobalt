#!/usr/bin/env vpython3
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import unittest
import tempfile

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# CIPD client version to use for self-update from an "old" checkout to the tip.
#
# This version is from Aug 2018. Digests were generated using:
#   cipd selfupdate-roll -version-file tmp \
#        -version git_revision:ea6c07cfcb596be6b63a1e6deb95bba79524b0c8
#   cat tmp.digests
OLD_VERSION = 'git_revision:ea6c07cfcb596be6b63a1e6deb95bba79524b0c8'
OLD_DIGESTS = """
linux-386       sha256  ee90bd655b90baf7586ab80c289c00233b96bfac3fa70e64cc5c48feb1998971
linux-amd64     sha256  73bd62cb72cde6f12d9b42cda12941c53e1e21686f6f2b1cd98db5c6718b7bed
linux-arm64     sha256  1f2619f3e7f5f6876d0a446bacc6cc61eb32ca1464315d7230034a832500ed64
linux-armv6l    sha256  98c873097c460fe8f6b4311f6e00b4df41ca50e9bd2d26f06995913a9d647d3a
linux-mips64    sha256  05e37c85502eb2b72abd8a51ff13a4914c5e071e25326c9c8fc257290749138a
linux-mips64le  sha256  5b3af8be6ea8a62662006f1a86fdc387dc765edace9f530acbeca77c0850a32d
linux-mipsle    sha256  cfa6539af00db69b7da00d46316f1aaaa90b38a5e6b33ce4823be17533e71810
linux-ppc64     sha256  faa49f2b59a25134e8a13b68f5addb00c434c7feeee03940413917eca1d333e6
linux-ppc64le   sha256  6fa51348e6039b864171426b02cfbfa1d533b9f86e3c72875e0ed116994a2fec
linux-s390x     sha256  6cd4bfff7e2025f2d3da55013036e39eea4e8f631060a5e2b32b9975fab08b0e
mac-amd64       sha256  6427b87fdaa1615a229d45c2fab1ba7fdb748ce785f2c09cd6e10adc48c58a66
windows-386     sha256  809c727a31e5f8c34656061b96839fbca63833140b90cab8e2491137d6e4fc4c
windows-amd64   sha256  3e21561b45acb2845c309a04cbedb2ce1e0567b7b24bf89857e7673607b09216
"""


class CipdBootstrapTest(unittest.TestCase):
  """Tests that CIPD client can bootstrap from scratch and self-update from some
  old version to a most recent one.

  WARNING: This integration test touches real network and real CIPD backend and
  downloads several megabytes of stuff.
  """

  def setUp(self):
    self.tempdir = tempfile.mkdtemp('depot_tools_cipd')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def stage_files(self, cipd_version=None, digests=None):
    """Copies files needed for cipd bootstrap into the temp dir.

    Args:
      cipd_version: if not None, a value to put into cipd_client_version file.
    """
    names = (
      '.cipd_impl.ps1',
      'cipd',
      'cipd.bat',
      'cipd_client_version',
      'cipd_client_version.digests',
    )
    for f in names:
      shutil.copy2(os.path.join(ROOT_DIR, f), os.path.join(self.tempdir, f))
    if cipd_version is not None:
      with open(os.path.join(self.tempdir, 'cipd_client_version'), 'wt') as f:
        f.write(cipd_version+'\n')
    if digests is not None:
      p = os.path.join(self.tempdir, 'cipd_client_version.digests')
      with open(p, 'wt') as f:
        f.write(digests+'\n')

  def call_cipd_help(self):
    """Calls 'cipd help' bootstrapping the client in tempdir.

    Returns (exit code, merged stdout and stderr).
    """
    exe = 'cipd.bat' if sys.platform == 'win32' else 'cipd'
    p = subprocess.Popen(
        [os.path.join(self.tempdir, exe), 'help'],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out, _ = p.communicate()
    return p.returncode, out

  def test_new_bootstrap(self):
    """Bootstrapping the client from scratch."""
    self.stage_files()
    ret, out = self.call_cipd_help()
    if ret:
      self.fail('Bootstrap from scratch failed:\n%s' % out)

  def test_self_update(self):
    """Updating the existing client in-place."""
    self.stage_files(cipd_version=OLD_VERSION, digests=OLD_DIGESTS)
    ret, out = self.call_cipd_help()
    if ret:
      self.fail('Update to %s fails:\n%s' % (OLD_VERSION, out))
    self.stage_files()
    ret, out = self.call_cipd_help()
    if ret:
      self.fail('Update from %s to the tip fails:\n%s' % (OLD_VERSION, out))


if __name__ == '__main__':
  unittest.main()
