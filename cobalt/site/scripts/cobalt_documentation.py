# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Module to copy Cobalt doc folders to cobalt site directory."""
import environment
import os
import shutil
import sys

_COBALT_DOC_LOCATIONS = [
    'cobalt/doc/',
    'starboard/doc/',
    'starboard/build/doc/',
    'starboard/tools/doc/',
]


def write_header(doc):
  doc.write('Project: /youtube/cobalt/_project.yaml\n')
  doc.write('Book: /youtube/cobalt/_book.yaml\n\n')


def copy_doc_locations(source_dir, output_dir=None):
  if output_dir:
    site_path = environment.get_site_dir(output_dir)
  else:
    site_path = environment.get_site_dir(source_dir)

  gen_dir = os.path.join(site_path, 'docs', 'gen')
  if os.path.exists(gen_dir):
    shutil.rmtree(gen_dir)

  for location in _COBALT_DOC_LOCATIONS:
    shutil.copytree(
        os.path.join(source_dir, location), os.path.join(gen_dir, location))

  for root, _, files in os.walk(gen_dir):
    for filename in files:
      if not filename.endswith('.md'):
        continue
      filename = os.path.join(root, filename)
      with open(filename, encoding='utf8') as f:
        lines = f.readlines()
      with open(filename, 'w', encoding='utf8') as f:
        write_header(f)
        f.writelines(lines)


if __name__ == '__main__':
  out = sys.argv[2] if len(sys.argv) == 3 else None
  copy_doc_locations(sys.argv[1], out)
