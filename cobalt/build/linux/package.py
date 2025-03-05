#!/usr/bin/env python3
"""Packages Cobalt for Linux."""

import os
from cobalt.build import packaging


def lay_out(out_dir, base_dir):
  files = [
      'gen/build_info.json',
      'chromedriver',
      'cobalt',
      'content_shell.pak',
      'icudtl.dat',
      'starboard/libstarboard.so.17',
      'libEGL.so',
      'libGLESv2.so',
      'libvk_swiftshader.so',
      'libvulkan.so.1',
      'v8_context_snapshot.bin',
      'vk_swiftshader_icd.json',
  ]
  for file in files:
    packaging.copy(
        os.path.join(out_dir, file),
        os.path.join(base_dir, os.path.basename(file)))


if __name__ == '__main__':
  packaging.run(lay_out)
