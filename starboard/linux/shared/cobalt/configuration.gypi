# Copyright 2017 Google Inc. All Rights Reserved.
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

# Cobalt-on-Linux-specific configuration.

{
  'variables': {
    'in_app_dial%': 1,
    'cobalt_media_source_2016': 1,
    # The maximum amount of memory that will be used to store media buffers when
    # video resolution is no larger than 1080p.
    'cobalt_media_buffer_max_capacity_1080p': 36 * 1024 * 1024,
    # The maximum amount of memory that will be used to store media buffers when
    # video resolution is 4k.
    'cobalt_media_buffer_max_capacity_4k': 65 * 1024 * 1024,
  }, # end of variables
}
