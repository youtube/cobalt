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
      ['javascript_engine == "javascriptcore"', {
        'generated_bindings_prefix': 'JSC',
        'engine_include_dirs': [
          '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore',
          '<(DEPTH)/third_party/WebKit/Source/WTF',
        ],
        'engine_dependencies': [
          '<(DEPTH)/third_party/WebKit/Source/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:javascriptcore',
        ],
        'engine_defines': [
          # Avoid WTF LOG macro.
          '__DISABLE_WTF_LOGGING__',
        ],
        'engine_templates_dir': [
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates',
        ],
        'engine_template_files': [
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/callback-interface.cc.template',
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/callback-interface.h.template',
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/interface.cc.template',
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/interface.h.template',
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/interface-object.template',
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/macros.cc.template',
          '<(DEPTH)/cobalt/bindings/javascriptcore/templates/prototype-object.template',
        ],
        'engine_bindings_scripts': [
          '<(DEPTH)/cobalt/bindings/javascriptcore/code_generator.py',
          '<(DEPTH)/cobalt/bindings/javascriptcore/idl_compiler.py',
        ],
        'engine_idl_compiler': '<(DEPTH)/cobalt/bindings/javascriptcore/idl_compiler.py',
      }],
    ],
  },
}
