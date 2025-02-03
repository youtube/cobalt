#!/usr/bin/env python3
"""Packages Cobalt for Android."""

import os
import shutil
from cobalt.build import packaging


def lay_out(out_dir, base_dir):
  shutil.copytree(out_dir, base_dir, dirs_exist_ok=True)
  shutil.rmtree(os.path.join(base_dir, 'lib.unstripped'))
  shutil.rmtree(os.path.join(base_dir, 'thinlto-cache'))


if __name__ == '__main__':
  packaging.run(lay_out)
