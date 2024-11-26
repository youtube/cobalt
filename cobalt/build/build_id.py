# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
"""Generates a Cobalt build ID header."""

import os
import sys

sys.path.insert(0,
                os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
# pylint: disable=wrong-import-position
from cobalt.build import build_info

BUILD_ID_HEADER_TEMPLATE = """
#ifndef _COBALT_BUILD_ID_H_
#define _COBALT_BUILD_ID_H_

#ifndef COBALT_BUILD_VERSION_NUMBER
#define COBALT_BUILD_VERSION_NUMBER "{cobalt_build_version_number}"
#endif  // COBALT_BUILD_VERSION_NUMBER


#endif  // _COBALT_BUILD_ID_H_
"""


def BuildId(output_path):
  """Writes a cobalt_build_id header file.

  Args:
    output_path: Location to write Cobalt build ID header to.
  Returns:
    0 on success.
  """
  with open(output_path, 'w', encoding='utf-8') as f:
    f.write(
        BUILD_ID_HEADER_TEMPLATE.format(
            cobalt_build_version_number=build_info.get_build_id()))


if __name__ == '__main__':
  BuildId(sys.argv[1])
