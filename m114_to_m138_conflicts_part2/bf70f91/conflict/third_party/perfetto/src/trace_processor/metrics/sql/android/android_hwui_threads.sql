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


DROP VIEW IF EXISTS {{table_name_prefix}}_main_thread;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_main_thread AS
=======
CREATE VIEW {{table_name_prefix}}_main_thread AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process.name AS process_name,
  thread.utid
FROM thread
JOIN {{process_allowlist_table}} process_allowlist USING (upid)
JOIN process USING (upid)
WHERE thread.is_main_thread;

DROP VIEW IF EXISTS {{table_name_prefix}}_render_thread;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_render_thread AS
=======
CREATE VIEW {{table_name_prefix}}_render_thread AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process.name AS process_name,
  thread.utid
FROM thread
JOIN {{process_allowlist_table}} process_allowlist USING (upid)
JOIN process USING (upid)
WHERE thread.name = 'RenderThread';

DROP VIEW IF EXISTS {{table_name_prefix}}_gpu_completion_thread;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_gpu_completion_thread AS
=======
CREATE VIEW {{table_name_prefix}}_gpu_completion_thread AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process.name AS process_name,
  thread.utid
FROM thread
JOIN {{process_allowlist_table}} process_allowlist USING (upid)
JOIN process USING (upid)
WHERE thread.name = 'GPU completion';

DROP VIEW IF EXISTS {{table_name_prefix}}_hwc_release_thread;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_hwc_release_thread AS
=======
CREATE VIEW {{table_name_prefix}}_hwc_release_thread AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process.name AS process_name,
  thread.utid
FROM thread
JOIN {{process_allowlist_table}} process_allowlist USING (upid)
JOIN process USING (upid)
WHERE thread.name = 'HWC release';

DROP TABLE IF EXISTS {{table_name_prefix}}_main_thread_slices;
<<<<<<< HEAD
CREATE PERFETTO TABLE {{table_name_prefix}}_main_thread_slices AS
=======
CREATE TABLE {{table_name_prefix}}_main_thread_slices AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process_name,
  thread.utid,
  slice.*,
  ts + dur AS ts_end
FROM slice
JOIN thread_track ON slice.track_id = thread_track.id
JOIN {{table_name_prefix}}_main_thread thread USING (utid)
WHERE dur > 0;

DROP VIEW IF EXISTS {{table_name_prefix}}_do_frame_slices;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_do_frame_slices AS
=======
CREATE VIEW {{table_name_prefix}}_do_frame_slices AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  *,
  CAST(STR_SPLIT(name, ' ', 1) AS INTEGER) AS vsync
FROM {{table_name_prefix}}_main_thread_slices
WHERE name GLOB 'Choreographer#doFrame*';

DROP TABLE IF EXISTS {{table_name_prefix}}_render_thread_slices;
<<<<<<< HEAD
CREATE PERFETTO TABLE {{table_name_prefix}}_render_thread_slices AS
=======
CREATE TABLE {{table_name_prefix}}_render_thread_slices AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process_name,
  thread.utid,
  slice.*,
  ts + dur AS ts_end
FROM slice
JOIN thread_track ON slice.track_id = thread_track.id
JOIN {{table_name_prefix}}_render_thread thread USING (utid)
WHERE dur > 0;

DROP VIEW IF EXISTS {{table_name_prefix}}_draw_frame_slices;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_draw_frame_slices AS
=======
CREATE VIEW {{table_name_prefix}}_draw_frame_slices AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  *,
  CAST(STR_SPLIT(name, ' ', 1) AS INTEGER) AS vsync
FROM {{table_name_prefix}}_render_thread_slices
WHERE name GLOB 'DrawFrame*';

DROP VIEW IF EXISTS {{table_name_prefix}}_gpu_completion_slices;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_gpu_completion_slices AS
=======
CREATE VIEW {{table_name_prefix}}_gpu_completion_slices AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process_name,
  thread.utid,
  slice.*,
  ts + dur AS ts_end,
  -- Extracts 1234 from 'waiting for GPU completion 1234'
  CAST(STR_SPLIT(slice.name, ' ', 4) AS INTEGER) AS idx
FROM slice
JOIN thread_track ON slice.track_id = thread_track.id
JOIN {{table_name_prefix}}_gpu_completion_thread thread USING (utid)
WHERE slice.name GLOB 'waiting for GPU completion *'
  AND dur > 0;

DROP VIEW IF EXISTS {{table_name_prefix}}_hwc_release_slices;
<<<<<<< HEAD
CREATE PERFETTO VIEW {{table_name_prefix}}_hwc_release_slices AS
=======
CREATE VIEW {{table_name_prefix}}_hwc_release_slices AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT
  process_name,
  thread.utid,
  slice.*,
  ts + dur AS ts_end,
  -- Extracts 1234 from 'waiting for HWC release 1234'
  CAST(STR_SPLIT(slice.name, ' ', 4) AS INTEGER) AS idx
FROM slice
JOIN thread_track ON slice.track_id = thread_track.id
JOIN {{table_name_prefix}}_hwc_release_thread thread USING (utid)
WHERE slice.name GLOB 'waiting for HWC release *'
  AND dur > 0;
