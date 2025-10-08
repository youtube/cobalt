--
-- Copyright 2022 The Android Open Source Project
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

DROP VIEW IF EXISTS chrome_user_event_hashes_output;

<<<<<<< HEAD
CREATE PERFETTO VIEW chrome_user_event_hashes_output AS
=======
CREATE VIEW chrome_user_event_hashes_output AS
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
SELECT ChromeUserEventHashes(
  'action_hash', (
    SELECT RepeatedField(int_value)
    FROM args
    WHERE key = 'chrome_user_event.action_hash'
    ORDER BY int_value
  )
);
