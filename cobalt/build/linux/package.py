#!/usr/bin/env python3
"""Packages Cobalt for Linux."""

import os
import shutil
from cobalt.build import packaging


def lay_out(out_dir, base_dir):
  place_in_base_dir = [
      'cobalt',
      'content_shell.pak',
      'icudtl.dat',
      'libEGL.so',
      'libGLESv2.so',
      'starboard/libstarboard.so.17',
      'v8_context_snapshot.bin',
  ]
  for f in place_in_base_dir:
    shutil.copy2(os.path.join(out_dir, f), base_dir)


if __name__ == '__main__':
  packaging.run(lay_out)
