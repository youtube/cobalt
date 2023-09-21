# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Android arm Cobalt configuration."""

from starboard.android.shared.cobalt import configuration
from starboard.tools.testing import test_filter

# A map of failing or crashing tests per target
_FILTERED_TESTS = {
    'blackbox': [
        'web_debugger',
        'cancel_sync_loads_when_suspended',
        'preload_font',
        'preload_visibility',
        'preload_launch_parameter',
        'suspend_visibility',
        'timer_hit_after_preload',
        'timer_hit_in_preload',
        'service_worker_add_to_cache_test',
        'service_worker_cache_keys_test',
        'service_worker_controller_activation_test',
        'service_worker_get_registrations_test',
        'service_worker_fetch_main_resource_test',
        'service_worker_fetch_test',
        'service_worker_message_test',
        'service_worker_post_message_test',
        'service_worker_test',
        'service_worker_persist_test',
        'deep_links',
        'web_platform_tests',
        'persistent_cookie',
    ],
}


class CobaltAndroidArmConfiguration(configuration.CobaltAndroidConfiguration):
  """Starboard Android Arm Cobalt configuration."""

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
