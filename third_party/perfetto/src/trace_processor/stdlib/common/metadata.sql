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

-- Extracts an int value with the given name from the metadata table.
--
-- @arg name STRING The name of the metadata entry.
-- @ret LONG int_value for the given name. NULL if there's no such entry.
SELECT
    CREATE_FUNCTION(
        'EXTRACT_INT_METADATA(name STRING)',
        'LONG',
        'SELECT int_value FROM metadata WHERE name = ($name)'
    );