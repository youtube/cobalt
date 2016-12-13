# Copyright 2016 Google Inc. All Rights Reserved.
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
    'conditions': [
      ['javascript_engine == "mozjs"', {
        'generated_bindings_prefix': 'Mozjs',
        'engine_include_dirs': [],
        'engine_dependencies': [
          '<(DEPTH)/third_party/mozjs/mozjs.gyp:mozjs_lib',
        ],
        'engine_defines': [],
        'engine_templates_dir': [
          '<(DEPTH)/cobalt/bindings/mozjs/templates',
        ],
        'engine_template_files': [
          '<(DEPTH)/cobalt/bindings/mozjs/templates/callback-interface.cc.template',
          '<(DEPTH)/cobalt/bindings/mozjs/templates/callback-interface.h.template',
          '<(DEPTH)/cobalt/bindings/mozjs/templates/interface.cc.template',
          '<(DEPTH)/cobalt/bindings/mozjs/templates/interface.h.template',
          '<(DEPTH)/cobalt/bindings/mozjs/templates/macros.cc.template',
        ],
        'engine_bindings_scripts': [
          '<(DEPTH)/cobalt/bindings/mozjs/code_generator.py',
          '<(DEPTH)/cobalt/bindings/mozjs/idl_compiler.py',
        ],
        'engine_idl_compiler': '<(DEPTH)/cobalt/bindings/mozjs/idl_compiler.py',
      }],
    ],
  },
}
