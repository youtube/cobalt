# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Cobalt YouTube Living Room stories for performance and memory tests."""

import logging

from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story


class _CobaltLivingRoomStoryBase(system_health_story.SystemHealthStory):
  """Base story for Cobalt YouTube Living Room performance and memory tests."""
  ABSTRACT_STORY = True
  URL = 'https://www.youtube.com/tv#/'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  TAGS = [story_tags.YEAR_2020]

  def RunNavigateSteps(self, action_runner):
    # Cobalt launches directly into YouTube Living Room on startup.
    # Avoid secondary navigation that causes render frame churn.
    action_runner.Wait(8)

  def _ScrollShelves(self, action_runner, num_shelves=4, cards_per_shelf=8):
    # Wait 8s for living room UI and images to settle
    action_runner.Wait(8)
    for shelf in range(num_shelves):
      # Scroll across cards on current shelf to decode and cache thumbnails
      action_runner.PressKey('ArrowRight', cards_per_shelf, 300)
      action_runner.Wait(1)
      if shelf < num_shelves - 1:
        # Move down to next shelf
        action_runner.PressKey('ArrowDown', 1, 500)
        action_runner.Wait(1)

  def _TriggerMemoryPressure(self, action_runner):
    # 1. Trigger Memory.simulatePressureNotification via CDP WebSocket
    try:
      inspector = getattr(action_runner.tab, '_inspector_backend', None)
      ws = getattr(inspector, '_websocket', None)
      if ws:
        ws.SendAndIgnoreResponse({
            'method': 'Memory.simulatePressureNotification',
            'params': {
                'level': 'critical'
            }
        })
        logging.info(
            'Triggered CDP Memory.simulatePressureNotification(critical)')
    except Exception as e:  # pylint: disable=broad-exception-caught
      logging.warning('CDP Memory.simulatePressureNotification error: %s', e)

    # 2. Trigger OS-level memory trim via Android ActivityManager
    try:
      platform_backend = getattr(action_runner.tab.browser.platform,
                                 '_platform_backend', None)
      device = getattr(platform_backend, 'device', None)
      if device:
        device.RunShellCommand(
            ['am', 'send-trim-memory', 'dev.cobalt.coat', 'RUNNING_CRITICAL'],
            check_return=False)
        logging.info(
            'Triggered Android OS am send-trim-memory RUNNING_CRITICAL')
    except Exception as e:  # pylint: disable=broad-exception-caught
      logging.warning('ADB send-trim-memory error: %s', e)


class CobaltLivingRoomScrollStory(_CobaltLivingRoomStoryBase):
  """Browse YouTube living room shelves to load image thumbnails.

  Serves as the peak memory baseline.
  """
  ABSTRACT_STORY = False
  NAME = 'browse:media:cobalt_livingroom:scroll'

  def _DidLoadDocument(self, action_runner):
    self._ScrollShelves(action_runner, num_shelves=4, cards_per_shelf=8)
    action_runner.Wait(3)


class CobaltLivingRoomReclaimStory(_CobaltLivingRoomStoryBase):
  """Browse YouTube living room shelves and trigger memory pressure.

  Verifies memory reclamation under critical pressure.
  """
  ABSTRACT_STORY = False
  NAME = 'browse:media:cobalt_livingroom:reclaim'

  def _DidLoadDocument(self, action_runner):
    self._ScrollShelves(action_runner, num_shelves=4, cards_per_shelf=8)
    action_runner.Wait(2)
    self._TriggerMemoryPressure(action_runner)
    action_runner.Wait(3)


class CobaltLivingRoomStory(CobaltLivingRoomScrollStory):
  """Browse YouTube living room experience without requiring login."""
  ABSTRACT_STORY = False
  NAME = 'browse:media:cobalt_livingroom'
