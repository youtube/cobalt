--
-- Copyright 2020 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the 'License');
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an 'AS IS' BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
-- While handling a InputLatency::GestureScrollUpdate event a sequence of Flows
-- define the critical path from Beginning to End. This metric breaks down the
-- flows for the same InputLatency::GestureScrollUpdate event.
--
-- WARNING: This metric should not be used as a source of truth. It is under
--          active development and the values & meaning might change without
--          notice.

-- Provides the scroll_jank table which gives us all the GestureScrollUpdate
-- events we care about and labels them janky or not.
SELECT RUN_METRIC(
    'chrome/gesture_flow_event.sql',
    'prefix', 'scroll',
    'gesture_update', 'GestureScrollUpdate',
    'id_field', 'gesture_scroll_id'
);
