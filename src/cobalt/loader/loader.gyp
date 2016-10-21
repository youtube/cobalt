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
        'embedded_fetcher.cc',
        'embedded_fetcher.h',
        'fetcher.cc',
        'fetcher.h',
        'fetcher_factory.cc',
        'fetcher_factory.h',
        'file_fetcher.cc',
        'file_fetcher.h',
        'font/typeface_decoder.cc',
        'font/typeface_decoder.h',
        'font/remote_typeface_cache.h',
        'image/dummy_gif_image_decoder.cc',
        'image/dummy_gif_image_decoder.h',
        'image/image_cache.h',
        'image/image_data_decoder.cc',
        'image/image_data_decoder.h',
        'image/image_decoder.cc',
        'image/image_decoder.h',
        'image/jpeg_image_decoder.cc',
        'image/jpeg_image_decoder.h',
        'image/png_image_decoder.cc',
        'image/png_image_decoder.h',
        'image/stub_image_decoder.h',
        'image/webp_image_decoder.cc',
        'image/webp_image_decoder.h',
        'loader.cc',
        'loader.h',
        'loader_factory.cc',
        'loader_factory.h',
        'loader_types.h',
        'net_fetcher.cc',
        'net_fetcher.h',
        'resource_cache.h',
        'sync_loader.cc',
        'sync_loader.h',
        'text_decoder.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/csp/csp.gyp:csp',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
        'embed_resources_as_header_files',
      ],
      'conditions': [
        ['target_arch == "ps3"', {
          'sources': [
            'image/jpeg_image_decoder_ps3.cc',
            'image/jpeg_image_decoder_ps3.h',
          ],
        }],
        ['enable_about_scheme == 1', {
          'defines': [ 'ENABLE_ABOUT_SCHEME' ],
          'sources': [
            'about_fetcher.cc',
            'about_fetcher.h',
          ]
        }],
      ],
    },

    {
      'target_name': 'loader_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'fetcher_factory_test.cc',
        'file_fetcher_test.cc',
        'font/typeface_decoder_test.cc',
        'image/image_decoder_test.cc',
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
      'includes': [ '../../starboard/build/deploy.gypi' ],
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
          'includes': [ '../build/copy_test_data.gypi' ],
        },
      ],
    },

    {
      # This target takes all files in the embedded_resources directory (e.g.
      # the splash screen) and embeds them as header files for
      # inclusion into the binary.
      'target_name': 'embed_resources_as_header_files',
      'type': 'none',
      # Because we generate a header, we must set the hard_dependency flag.
      'hard_dependency': 1,
      'variables': {
        'script_path': '<(DEPTH)/cobalt/build/generate_data_header.py',
        'output_path': '<(SHARED_INTERMEDIATE_DIR)/cobalt/loader/embedded_resources.h',
        'input_directory': 'embedded_resources',
      },
      'sources': [
        '<(input_directory)/splash_screen.css',
        '<(input_directory)/splash_screen.html',
        '<(input_directory)/you_tube_logo.png',
      ],
      'actions': [
        {
          'action_name': 'embed_resources_as_header_files',
          'inputs': [
            '<(script_path)',
            '<@(_sources)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': ['python', '<(script_path)', 'LoaderEmbeddedResources', '<(input_directory)', '<(output_path)'],
          'message': 'Embedding layout resources in "<(input_directory)" into header file, "<(output_path)".',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ],
}
