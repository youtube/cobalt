# Copyright 2015 Google Inc. All Rights Reserved.
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
    'cobalt_code': 1,
  },
  'targets': [
    {
      'target_name': 'loader',
      'type': 'static_library',
      'sources': [
        'decoder.h',
        'fetcher.cc',
        'fetcher.h',
        'fetcher_factory.cc',
        'fetcher_factory.h',
        'file_fetcher.cc',
        'file_fetcher.h',
        'image_cache.cc',
        'image_cache.h',
        'image_decoder/any_image_decoder.cc',
        'image_decoder/any_image_decoder.h',
        'image_decoder/image_decoder.h',
        'image_decoder/jpeg_image_decoder.cc',
        'image_decoder/jpeg_image_decoder.h',
        'image_decoder/png_image_decoder.cc',
        'image_decoder/png_image_decoder.h',
        'loader.cc',
        'loader.h',
        'net_fetcher.cc',
        'net_fetcher.h',
        'text_decoder.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
    },

    {
      'target_name': 'loader_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'fetcher_factory_test.cc',
        'file_fetcher_test.cc',
        'image_decoder/any_image_decoder_test.cc',
        'loader_test.cc',
        'text_decoder_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'loader',
        'loader_copy_test_data',
      ],
    },

    {
      'target_name': 'loader_test_deploy',
      'type': 'none',
      'dependencies': [
        'loader_test',
      ],
      'variables': {
        'executable_name': 'loader_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },

    {
      'target_name': 'loader_copy_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'loader_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/loader/testdata/',
            ],
            'output_dir': 'cobalt/loader/testdata/',
          },
          'includes': [ '../build/copy_data.gypi' ],
        },
      ],
    },
  ],
}
