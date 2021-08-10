# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
    'generated_bindings_prefix': 'v8c',
    'engine_include_dirs': [],
    'engine_dependencies': [
      '<(DEPTH)/third_party/v8/v8.gyp:v8',
      '<(DEPTH)/third_party/v8/v8.gyp:v8_libplatform',
      '<(DEPTH)/net/net.gyp:net',
    ],
    'engine_defines': [],
    'engine_templates_dir': [
      '<(DEPTH)/cobalt/bindings/v8c/templates',
    ],
    'engine_template_files': [
      '<(DEPTH)/cobalt/bindings/v8c/templates/callback-interface.cc.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/callback-interface.h.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/dictionary-conversion.cc.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/enumeration-conversion.cc.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/generated-types.h.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/interface.cc.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/interface.h.template',
      '<(DEPTH)/cobalt/bindings/v8c/templates/macros.cc.template',
    ],
    'engine_bindings_scripts': [
      '<(DEPTH)/cobalt/bindings/v8c/code_generator_v8c.py',
      '<(DEPTH)/cobalt/bindings/v8c/idl_compiler_v8c.py',
      '<(DEPTH)/cobalt/bindings/v8c/generate_conversion_header_v8c.py',
    ],
    'engine_idl_compiler':
        '<(DEPTH)/cobalt/bindings/v8c/idl_compiler_v8c.py',
    'engine_conversion_header_generator_script':
        '<(DEPTH)/cobalt/bindings/v8c/generate_conversion_header_v8c.py',
  },
}
