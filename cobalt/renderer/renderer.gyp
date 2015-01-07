# Copyright 2014 Google Inc. All Rights Reserved.
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
      # This target conveniently aggregates all sub-modules required to declare
      # and implement rasterization commands as well as display/GPU resource
      # management and communication.  It will eventually also contain source
      # code to help glue all of the components together.
      'target_name': 'renderer',
      'type': 'static_library',
      'sources': [
        'pipeline.cc',
        'pipeline.h',
        'rasterizer.h',
        'render_tree_backend_conversions.cc',
        'render_tree_backend_conversions.h',
        'renderer_module.cc',
        'renderer_module.h',
        'resource_provider.h',
      ],

      'includes': [
        'copy_font_data.gypi',
        'rasterizer_skia/rasterizer_skia.gypi'
      ],

      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/cobalt/renderer/backend/backend.gyp:renderer_backend',
      ],
    },

    {
      'target_name': 'renderer_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'pipeline_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'renderer',
      ],
    },

    {
      'target_name': 'renderer_test_deploy',
      'type': 'none',
      'dependencies': [
        'renderer_test',
      ],
      'variables': {
        'executable_name': 'renderer_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
