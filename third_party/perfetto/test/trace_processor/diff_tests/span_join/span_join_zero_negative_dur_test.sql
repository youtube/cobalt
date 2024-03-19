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
CREATE TABLE t1(
  ts BIGINT,
  dur BIGINT,
  part BIGINT,
  PRIMARY KEY (part, ts, dur)
) WITHOUT ROWID;

INSERT INTO t1(ts, dur, part)
VALUES
(1, 0, 0),
(5, -1, 0),
(2, 0, 1);

CREATE TABLE t2(
  ts BIGINT,
  dur BIGINT,
  part BIGINT,
  PRIMARY KEY (part, ts, dur)
) WITHOUT ROWID;

INSERT INTO t2(ts, dur, part)
VALUES
(1, 2, 0),
(5, 0, 0),
(1, 1, 1);

CREATE VIRTUAL TABLE sp USING span_outer_join(t1 PARTITIONED part, t2 PARTITIONED part);

SELECT ts, dur, part FROM sp;
