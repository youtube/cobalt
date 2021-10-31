# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'dom_parser',
      'type': 'static_library',
      'sources': [
        'html_decoder.cc',
        'html_decoder.h',
        'libxml_html_parser_wrapper.cc',
        'libxml_html_parser_wrapper.h',
        'libxml_parser_wrapper.cc',
        'libxml_parser_wrapper.h',
        'libxml_xml_parser_wrapper.cc',
        'libxml_xml_parser_wrapper.h',
        'parser.cc',
        'parser.h',
        'xml_decoder.cc',
        'xml_decoder.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/loader/loader.gyp:loader',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
      ],
    },
  ],
}
