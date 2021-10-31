# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

{
  'targets': [
    {
      'target_name': 'lz4',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/third_party/lz4_lib',
      ],
      'defines': [
        # Make LZ4 use our implementations for its memory management functions.
        'LZ4_USER_MEMORY_FUNCTIONS',
        'XXH_NAMESPACE=LZ4_',
      ],
      'sources': [
        '<(DEPTH)/third_party/lz4_lib/lz4_all.c',
        '<(DEPTH)/third_party/lz4_lib/lz4starboard.c',
        '<(DEPTH)/third_party/lz4_lib/xxhash.c',
      ],
    },
  ],
}
