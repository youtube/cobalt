--
-- Copyright 2019 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
<<<<<<< HEAD
INCLUDE PERFETTO MODULE android.suspend;

DROP VIEW IF EXISTS trace_metadata_output;
CREATE PERFETTO VIEW trace_metadata_output AS
SELECT TraceMetadata(
  'trace_duration_ns', CAST(trace_dur() AS INT),
=======

-- Expose all clock snapshots as instant events.
DROP VIEW IF EXISTS trace_metadata_event;
CREATE VIEW trace_metadata_event AS
SELECT
  'slice' AS track_type,
  'Clock Snapshots' AS track_name,
  ts,
  0 AS dur,
  'Snapshot' AS slice_name
FROM clock_snapshot
GROUP BY ts;

DROP VIEW IF EXISTS trace_metadata_output;
CREATE VIEW trace_metadata_output AS
SELECT TraceMetadata(
  'trace_duration_ns', CAST((SELECT end_ts - start_ts FROM trace_bounds) AS INT),
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  'trace_uuid', (SELECT str_value FROM metadata WHERE name = 'trace_uuid'),
  'android_build_fingerprint', (
    SELECT str_value FROM metadata WHERE name = 'android_build_fingerprint'
  ),
<<<<<<< HEAD
  'android_device_manufacturer', (
    SELECT str_value FROM metadata WHERE name = 'android_device_manufacturer'
  ),
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  'statsd_triggering_subscription_id', (
    SELECT int_value FROM metadata
    WHERE name = 'statsd_triggering_subscription_id'
  ),
  'unique_session_name', (
    SELECT str_value FROM metadata
    WHERE name = 'unique_session_name'
  ),
  'trace_size_bytes', (
    SELECT int_value FROM metadata
    WHERE name = 'trace_size_bytes'
  ),
  'trace_trigger', (
    SELECT RepeatedField(slice.name)
    FROM track JOIN slice ON track.id = slice.track_id
    WHERE track.name = 'Trace Triggers'
  ),
<<<<<<< HEAD
  'trace_causal_trigger', (
      SELECT str_value FROM metadata
      WHERE name = 'trace_trigger'
  ),
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  'trace_config_pbtxt', (
    SELECT str_value FROM metadata
    WHERE name = 'trace_config_pbtxt'
  ),
  'sched_duration_ns', (
    SELECT MAX(ts) - MIN(ts) FROM sched
<<<<<<< HEAD
  ),
  'tracing_started_ns', (
    SELECT int_value FROM metadata
    WHERE name='tracing_started_ns'
  ),
  'android_sdk_version', (
    SELECT int_value FROM metadata
    WHERE name = 'android_sdk_version'
  ),
  'android_profile_boot_classpath', (
    SELECT int_value FROM metadata
    WHERE name = 'android_profile_boot_classpath'
  ),
  'android_profile_system_server', (
    SELECT int_value FROM metadata
    WHERE name = 'android_profile_system_server'
  ),
  'suspend_count', (
    SELECT COUNT() FROM android_suspend_state WHERE power_state = 'suspended'
  ),
  'data_loss_count', (
      SELECT COUNT()
      FROM stats
      WHERE severity = 'data_loss' AND value > 0
  ),
  'error_count', (
      SELECT COUNT()
      FROM stats
      WHERE severity = 'error' AND value > 0
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  )
);
