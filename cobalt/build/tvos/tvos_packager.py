#!/usr/bin/env python3
#
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""tvOS packager to bundle the native cobalt_archive output and assets."""

import os
import shutil
import sys

if len(sys.argv) < 4:
  print('Usage: tvos_packager.py <out_dir> <package_dir> <build_info_path>')
  sys.exit(1)

out_dir = sys.argv[1]
package_dir = sys.argv[2]
build_info_path = sys.argv[3]

# 1. Copy libcobalt_archive.a compiled natively by Ninja
lib_dir = os.path.join(package_dir, 'lib')
os.makedirs(lib_dir, exist_ok=True)

archive_src = os.path.join(out_dir, 'obj/cobalt/libcobalt_archive.a')
archive_dest = os.path.join(lib_dir, 'cobalt.a')

if not os.path.exists(archive_src):
  print(f'Error: Native archive libcobalt_archive.a not found at '
        f'{archive_src}')
  sys.exit(1)

print(f'Copying cobalt archive to {archive_dest}...')
shutil.copy2(archive_src, archive_dest)

# 2. Copy 'content' tree (asset pack)
content_src = os.path.join(out_dir, 'install', 'usr', 'share', 'cobalt')
content_dest = os.path.join(package_dir, 'content')
if os.path.exists(content_dest):
  shutil.rmtree(content_dest)

if os.path.exists(content_src):
  print('Copying content tree...')
  shutil.copytree(content_src, content_dest)
else:
  print(f'Warning: Content tree not found at {content_src}')

# 3. Copy build_info.json
print('Copying build_info.json...')
shutil.copy2(build_info_path, os.path.join(package_dir, 'build_info.json'))

print('Success!')
