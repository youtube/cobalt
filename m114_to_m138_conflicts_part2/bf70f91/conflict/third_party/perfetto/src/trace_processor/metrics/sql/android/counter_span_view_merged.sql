--
-- Copyright 2020 The Android Open Source Project
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
-- Creates a span view for counters that may be global or associated with a
=======
-- Creates a span view for counters that may be global or associated with a 
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
-- process, assuming that in the latter case we don't actually care about the
-- process (probably because it's always system_server). We may want to erase
-- this distinction for example when merging system properties and atrace
-- counters.
--
-- It also does another type of merging: it merges together temporally adjacent
-- identical values.

<<<<<<< HEAD
--TODO(simonmacm) remove when not referenced internally
DROP VIEW IF EXISTS {{table_name}}_span;
CREATE PERFETTO VIEW {{table_name}}_span AS
SELECT
  ts,
  LEAD(ts, 1, trace_end()) OVER(ORDER BY ts) - ts AS dur,
=======
DROP VIEW IF EXISTS {{table_name}}_span;
CREATE VIEW {{table_name}}_span AS
SELECT
  ts,
  LEAD(ts, 1, (SELECT end_ts + 1 FROM trace_bounds))
  OVER(ORDER BY ts) - ts AS dur,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  CAST(value AS INT) AS {{table_name}}_val
FROM (
    SELECT ts, value, LAG(value) OVER (ORDER BY ts) AS lag_value
    FROM counter c JOIN counter_track t
      ON t.id = c.track_id
    WHERE name = '{{counter_name}}'
)
WHERE value != lag_value OR lag_value IS NULL;
