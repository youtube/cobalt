# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'benchmark',
      'type': '<(final_executable_type)',
      'defines': [
        # This allows the benchmarks to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
      'sources': [
        '<(DEPTH)/starboard/benchmark/memory_benchmark.cc',
        '<(DEPTH)/starboard/benchmark/thread_benchmark.cc',
        '<(DEPTH)/starboard/common/benchmark_main.cc',
      ],
      'dependencies': [
        '<@(cobalt_platform_dependencies)',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/google_benchmark/google_benchmark.gyp:google_benchmark',
      ],
    },
    {
      'target_name': 'benchmark_deploy',
      'type': 'none',
      'dependencies': [
        'benchmark',
      ],
      'variables': {
        'executable_name': 'benchmark',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
