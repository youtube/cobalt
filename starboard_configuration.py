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
# limitations under the License."""
"""Definition of interface used by host apps to configure Starboard"""

# List of paths to directories in which Starboard ports are located.
# Each path should be represented as a list of directory names whose locations
# are relative to the location of this file.
PORT_ROOTS = [
    ["starboard"],
    ["starboard", "port"],
    ["third_party", "starboard"]
]

#  Names of all the unit test binaries that can be run by
#  the Starboard unit test runner.
TEST_TARGETS = [
    'audio_test',
    'base_test',
    'base_unittests',
    'bindings_test',
    'crypto_unittests',
    'csp_test',
    'css_parser_test',
    'cssom_test',
    'dom_parser_test',
    'dom_test',
    'layout_test',
    'layout_tests',
    'loader_test',
    'math_test',
    'media_session_test',
    'nb_test',
    'nplb',
    'nplb_blitter_pixel_tests',
    'net_unittests',
    'network_test',
    'page_visibility_test',
    'poem_unittests',
    'render_tree_test',
    'renderer_test',
    'sql_unittests',
    'starboard_platform_tests',
    'storage_test',
    'trace_event_test',
    'web_animations_test',
    'web_platform_tests',
    'webdriver_test',
    'xhr_test',
]
