# Copyright 2016 The Cobalt Authors. All rights reserved.
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
  'variables': {
    'pkg-config': 'pkg-config',
  },
  'targets': [
    {
      'target_name': 'aul',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags aul)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other aul)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l aul)',
        ],
      },
    }, # aul
    {
      'target_name': 'capi-appfw-application',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags capi-appfw-application)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other capi-appfw-application)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l capi-appfw-application)',
        ],
      },
    }, # capi-appfw-application
    {
      'target_name': 'capi-media-audio-io',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags capi-media-audio-io)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other capi-media-audio-io)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l capi-media-audio-io)',
        ],
      },
    }, # capi-media-audio-io
    {
      'target_name': 'capi-network-connection',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags capi-network-connection)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other capi-network-connection)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l capi-network-connection)',
        ],
      },
    }, # capi-network-connection
    {
      'target_name': 'capi-system-info',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags capi-system-info)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other capi-system-info)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l capi-system-info)',
        ],
      },
    }, # capi-system-info
    {
      'target_name': 'edbus',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags edbus)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other edbus)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l edbus)',
        ],
      },
    }, # edbus
    {
      'target_name': 'elementary',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags elementary)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other elementary)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l elementary)',
        ],
      },
      'conditions': [
         ['clang==1', {
            'direct_dependent_settings': {
              'cflags': [
                 # Fix: elm_prefs_common.h:27:9: warning: empty struct has size 0 in C, size 1 in C++
                 '-Wno-extern-c-compat',
              ],
            },
         }],
      ],
    }, # elementary
    {
      'target_name': 'evas',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags evas)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other evas)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l evas)',
        ],
      },
    }, # evas
    {
      'target_name': 'gles20',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags gles20)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other gles20)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l gles20)',
        ],
      },
    }, # gles20
    {
      'target_name': 'tizen-extension-client',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags tizen-extension-client)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other tizen-extension-client)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l tizen-extension-client)',
        ],
      },
    }, # tizen-extension-client
    {
      'target_name': 'wayland-egl',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags wayland-egl)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other wayland-egl)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l wayland-egl)',
        ],
      },
    }, # wayland-egl
  ],
}
