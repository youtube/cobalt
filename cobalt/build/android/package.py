#!/usr/bin/env python3
"""Packages Cobalt for Android."""

import os
import shutil
from cobalt.build import packaging


def lay_out(out_dir, base_dir):
  shutil.copy2(os.path.join(out_dir, 'gen', 'build_info.json'), base_dir)
  shutil.copy2(os.path.join(out_dir, 'apks/Cobalt.apk'), base_dir)
  shutil.copytree(
      os.path.join(out_dir, 'content'),
      os.path.join(base_dir, 'cobalt_package', 'content'))


if __name__ == '__main__':
  packaging.run(lay_out)
