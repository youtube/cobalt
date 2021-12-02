#!/usr/bin/env python
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Prints out the Cobalt Build ID."""

import os
from gyp_utils import GetOrGenerateNewBuildNumber


def main():
  build_id_server_url = os.environ.get('BUILD_ID_SERVER_URL')
  if build_id_server_url:
    print(GetOrGenerateNewBuildNumber(version_server=build_id_server_url))
  else:
    # No need to generate a build id for local builds.
    print('0')


if __name__ == '__main__':
  main()
