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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'loader',
      'type': 'static_library',
      'sources': [
        'blob_fetcher.cc',
        'blob_fetcher.h',
        'cache_fetcher.cc',
        'cache_fetcher.h',
        'cors_preflight.cc',
        'cors_preflight.h',
        'cors_preflight_cache.cc',
        'cors_preflight_cache.h',
        'decoder.h',
        'embedded_fetcher.cc',
        'embedded_fetcher.h',
        'error_fetcher.cc',
        'error_fetcher.h',
        'fetcher_factory.cc',
        'fetcher_factory.h',
        'fetcher.cc',
        'fetcher.h',
        'file_fetcher.cc',
        'file_fetcher.h',
        'font/remote_typeface_cache.h',
        'font/typeface_decoder.cc',
        'font/typeface_decoder.h',
        'image/animated_image_tracker.cc',
        'image/animated_image_tracker.h',
        'image/animated_webp_image.cc',
        'image/animated_webp_image.h',
        'image/dummy_gif_image_decoder.cc',
        'image/dummy_gif_image_decoder.h',
        'image/image_cache.h',
        'image/image_data_decoder.cc',
        'image/image_data_decoder.h',
        'image/image_decoder_starboard.cc',
        'image/image_decoder_starboard.h',
        'image/image_decoder.cc',
        'image/image_decoder.h',
        'image/image.h',
        'image/jpeg_image_decoder.cc',
        'image/jpeg_image_decoder.h',
        'image/png_image_decoder.cc',
        'image/png_image_decoder.h',
        'image/stub_image_decoder.h',
        'image/threaded_image_decoder_proxy.cc',
        'image/threaded_image_decoder_proxy.h',
        'image/webp_image_decoder.cc',
        'image/webp_image_decoder.h',
        'loader_factory.cc',
        'loader_factory.h',
        'loader_types.h',
        'loader.cc',
        'loader.h',
        'mesh/mesh_cache.h',
        'mesh/mesh_decoder.cc',
        'mesh/mesh_decoder.h',
        'mesh/mesh_projection.h',
        'mesh/projection_codec/constants.h',
        'mesh/projection_codec/indexed_vert.cc',
        'mesh/projection_codec/indexed_vert.h',
        'mesh/projection_codec/projection_decoder.cc',
        'mesh/projection_codec/projection_decoder.h',
        'net_fetcher.cc',
        'net_fetcher.h',
        'origin.cc',
        'origin.h',
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
        ['enable_about_scheme == 1', {
          'defines': [ 'ENABLE_ABOUT_SCHEME' ],
          'sources': [
            'about_fetcher.cc',
            'about_fetcher.h',
          ]
        }],
        ['enable_xhr_header_filtering == 1', {
          'dependencies': [
            '<@(cobalt_platform_dependencies)',
          ],
          'defines': [
            'COBALT_ENABLE_XHR_HEADER_FILTERING',
          ],
          'direct_dependent_settings': {
            'defines': [
              'COBALT_ENABLE_XHR_HEADER_FILTERING',
            ],
          },
        }],
      ],
    },

    {
      'target_name': 'loader_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'blob_fetcher_test.cc',
        'fetcher_factory_test.cc',
        'fetcher_test.h',
        'file_fetcher_test.cc',
        'font/typeface_decoder_test.cc',
        'image/image_decoder_test.cc',
        'mesh/mesh_decoder_test.cc',
        'loader_test.cc',
        'text_decoder_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/ots/ots.gyp:ots',
        'loader',
        'loader_copy_test_data',
        '<@(cobalt_platform_dependencies)',
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
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/cobalt/loader/testdata/',
        ],
        'content_test_output_subdir': 'cobalt/loader/testdata/',
      },
      'includes': [ '<(DEPTH)/starboard/build/copy_test_data.gypi' ],
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
        '<(input_directory)/black_splash_screen.html',
        '<(input_directory)/cobalt_splash_screen.css',
        '<(input_directory)/cobalt_splash_screen.html',
        '<(input_directory)/equirectangular_40_40.msh',
        '<(input_directory)/splash_screen.js',
        '<!@(["python", "<(DEPTH)/starboard/tools/find_private_files.py", "<(DEPTH)", "*.html", "cobalt/loader/embedded_resources"])',
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
          'action': ['python', '<(script_path)', 'LoaderEmbeddedResources', '<(output_path)', '<(input_directory)'],
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
