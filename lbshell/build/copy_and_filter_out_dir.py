#!/usr/bin/env python3
#
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
"""Compatibility layer for buildbot."""

<<<<<<< HEAD:lbshell/build/copy_and_filter_out_dir.py
import sys
from tools.copy_and_filter_out_dir import main
=======
# TODO: b/315170518 - Revert to static library after fixing
# symbol visibility issues for windows based modular platform builds.
source_set("posix_time_wrappers") {
  sources = [
    "posix_time_wrappers.cc",
    "posix_time_wrappers.h",
  ]
>>>>>>> 649c19a331f (Make posix_time_wrappers a source_set (#2121)):starboard/shared/modular/BUILD.gn

if __name__ == '__main__':
  sys.exit(main())
