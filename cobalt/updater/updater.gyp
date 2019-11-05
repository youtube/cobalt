# Copyright 2019 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'updater',
      'type': 'static_library',
      'sources': [
        'configurator.cc',
        'configurator.h',
        'network_fetcher.cc',
        'network_fetcher.h',
        # TODO: enable crash report later with crashpad dependency
        #'crash_client.cc',
        #'crash_client.h',
        #'crash_reporter.cc',
        #'crash_reporter.h',
        'patcher.cc',
        'patcher.h',
        'prefs.cc',
        'prefs.h',
        'unzipper.cc',
        'unzipper.h',
        'updater.cc',
        'updater.h',
        'updater_constants.cc',
        'updater_constants.h',
        'util.cc',
        'util.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/cobalt/loader/loader.gyp:loader',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/components/prefs/prefs.gyp:prefs',
        '<(DEPTH)/components/update_client/update_client.gyp:update_client',
        '<(DEPTH)/third_party/zlib2/zlib2.gyp:zip',
        '<(DEPTH)/url/url.gyp:url',
      ],
    },
    {
      'target_name': 'updater_sandbox',
      'type': '<(final_executable_type)',
      'dependencies': [
        'updater',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/debug/debug.gyp:console_command_manager',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
      'sources': [
        'updater_sandbox.cc',
      ],
    },
  ]
}
