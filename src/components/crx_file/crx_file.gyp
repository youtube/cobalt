# Copyright 2019 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'crx_file',
      'type': 'static_library',
      'sources': [
        'crx_file.h',
        'crx_verifier.cc',
        'crx_verifier.h',
        'crx3.pb.cc',
        'crx3.pb.h',
        'crx3.proto',
        'id_util.cc',
        'id_util.h',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
    {
      'target_name': 'crx_creator',
      'type': 'static_library',
      'sources': [
        'crx_creator.cc',
        'crx_creator.h',
      ],
      'dependencies': [
        'crx_file',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
  ]
}
