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

"""Generate a Cobalt build ID header."""

import datetime
import os
import sys
import time

template = """
#ifndef _COBALT_BUILD_ID_H_
#define _COBALT_BUILD_ID_H_

#define COBALT_BUILD_VERSION_DATE "{date_rep}"
#define COBALT_BUILD_VERSION_NUMBER "{version_number}"
#define COBALT_BUILD_VERSION_TIMESTAMP "{timestamp}"
#define COBALT_BUILD_VERSION_USERNAME "{username}"

#endif  // _COBALT_BUILD_ID_H_
"""


def BuildId(output_path, version_number):
  """Write a Cobalt build_id header file with time and version info.

  Args:
    output_path: Location of the build id header to write.
    version_number: Build version number, generated when gyp_cobalt is run.
  Returns:
    0 on success.
  """
  username = os.environ.get('USERNAME', os.environ.get('USER'))
  if not username:
    username = 'unknown'
  timestamp = time.time()
  date_rep = datetime.datetime.fromtimestamp(timestamp).strftime('%c')

  with open(output_path, 'w') as f:
    f.write(template.format(
        date_rep=date_rep,
        timestamp=int(timestamp),
        version_number=version_number,
        username=username))
  return 0


if __name__ == '__main__':
  sys.exit(BuildId(sys.argv[1], sys.argv[2]))
