--
-- Copyright 2023 The Android Open Source Project
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

-- Extracts the blocking thread from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_blocking_thread(
  slice_name STRING
)
RETURNS STRING AS
SELECT STR_SPLIT(STR_SPLIT($slice_name, "with owner ", 1), " (", 0);

-- Extracts the blocking thread tid from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret INT                 Blocking thread tid
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_blocking_tid(
  slice_name STRING
)
RETURNS INT AS
SELECT CAST(STR_SPLIT(STR_SPLIT($slice_name, " (", 1), ")", 0) AS INT);

-- Extracts the blocking method from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_blocking_method(
  slice_name STRING
)
RETURNS STRING AS
SELECT STR_SPLIT(STR_SPLIT($slice_name, ") at ", 1), "(", 0)
    || "("
    || STR_SPLIT(STR_SPLIT($slice_name, ") at ", 1), "(", 1);

-- Extracts a shortened form of the blocking method name from a slice name.
-- The shortened form discards the parameter and return
-- types.
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_short_blocking_method(
  slice_name STRING
)
RETURNS STRING AS
SELECT
    STR_SPLIT(STR_SPLIT(android_extract_android_monitor_contention_blocking_method($slice_name), " ", 1), "(", 0);

-- Extracts the monitor contention blocked method from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_blocked_method(
  slice_name STRING
)
RETURNS STRING AS
SELECT STR_SPLIT(STR_SPLIT($slice_name, "blocking from ", 1), "(", 0)
    || "("
    || STR_SPLIT(STR_SPLIT($slice_name, "blocking from ", 1), "(", 1);

-- Extracts a shortened form of the monitor contention blocked method name
-- from a slice name. The shortened form discards the parameter and return
-- types.
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_short_blocked_method(
  slice_name STRING
)
RETURNS STRING AS
SELECT
    STR_SPLIT(STR_SPLIT(android_extract_android_monitor_contention_blocked_method($slice_name), " ", 1), "(", 0);

-- Extracts the number of waiters on the monitor from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret INT                 Count of waiters on the lock
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_waiter_count(
  slice_name STRING
)
RETURNS INT AS
SELECT CAST(STR_SPLIT(STR_SPLIT($slice_name, "waiters=", 1), " ", 0) AS INT);

-- Extracts the monitor contention blocking source location from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_blocking_src(
  slice_name STRING
)
RETURNS STRING AS
SELECT STR_SPLIT(STR_SPLIT($slice_name, ")(", 1), ")", 0);

-- Extracts the monitor contention blocked source location from a slice name
--
-- @arg slice_name STRING   Name of slice
-- @ret STRING              Blocking thread
CREATE PERFETTO FUNCTION android_extract_android_monitor_contention_blocked_src(
  slice_name STRING
)
RETURNS STRING AS
SELECT STR_SPLIT(STR_SPLIT($slice_name, ")(", 2), ")", 0);

CREATE PERFETTO TABLE internal_valid_android_monitor_contention AS
SELECT slice.id AS id
FROM slice
LEFT JOIN slice child
  ON child.parent_id = slice.id
LEFT JOIN slice grand_child
  ON grand_child.parent_id = child.id
WHERE
  slice.name GLOB 'monitor contention*'
  AND (child.name GLOB 'Lock contention*' OR child.name IS NULL)
  AND (grand_child.name IS NULL)
GROUP BY slice.id;

-- Contains parsed monitor contention slices.
--
-- @column blocking_method Name of the method holding the lock.
-- @column blocked_methhod Name of the method trying to acquire the lock.
-- @column short_blocking_method Blocking_method without arguments and return types.
-- @column short_blocked_method Blocked_method without arguments and return types.
-- @column blocking_src File location of blocking_method in form <filename:linenumber>.
-- @column blocked_src File location of blocked_method in form <filename:linenumber>.
-- @column waiter_count Zero indexed number of threads trying to acquire the lock.
-- @column blocking_utid Utid of thread holding the lock.
-- @column blocking_thread_name Thread name of thread holding the lock.
-- @column upid Upid of process experiencing lock contention.
-- @column process_name Process name of process experiencing lock contention.
-- @column id Slice id of lock contention.
-- @column ts Timestamp of lock contention start.
-- @column dur Duration of lock contention.
-- @column track_id Thread track id of blocked thread.
-- @column is_blocked_main_thread Whether the blocked thread is the main thread.
-- @column is_blocking_main_thread Whether the blocking thread is the main thread.
-- @column binder_reply_id Slice id of binder reply slice if lock contention was part of a binder txn.
-- @column binder_reply_ts Timestamp of binder reply slice if lock contention was part of a binder txn.
-- @column binder_reply_tid Tid of binder reply slice if lock contention was part of a binder txn.
CREATE TABLE android_monitor_contention AS
SELECT
  android_extract_android_monitor_contention_blocking_method(slice.name) AS blocking_method,
  android_extract_android_monitor_contention_blocked_method(slice.name)  AS blocked_method,
  android_extract_android_monitor_contention_short_blocking_method(slice.name) AS short_blocking_method,
  android_extract_android_monitor_contention_short_blocked_method(slice.name)  AS short_blocked_method,
  android_extract_android_monitor_contention_blocking_src(slice.name) AS blocking_src,
  android_extract_android_monitor_contention_blocked_src(slice.name) AS blocked_src,
  android_extract_android_monitor_contention_waiter_count(slice.name) AS waiter_count,
  thread.utid AS blocked_utid,
  thread.name AS blocked_thread_name,
  blocking_thread.utid AS blocking_utid,
  android_extract_android_monitor_contention_blocking_thread(slice.name) AS blocking_thread_name,
  android_extract_android_monitor_contention_blocking_tid(slice.name) AS blocking_tid,
  thread.upid AS upid,
  process.name AS process_name,
  slice.id,
  slice.ts,
  slice.dur,
  slice.track_id,
  thread.is_main_thread AS is_blocked_thread_main,
  thread.tid AS blocked_thread_tid,
  blocking_thread.is_main_thread AS is_blocking_thread_main,
  blocking_thread.tid AS blocking_thread_tid,
  binder_reply.id AS binder_reply_id,
  binder_reply.ts AS binder_reply_ts,
  binder_reply_thread.tid AS binder_reply_tid,
  process.pid
FROM slice
JOIN thread_track
  ON thread_track.id = slice.track_id
LEFT JOIN thread
  USING (utid)
LEFT JOIN process
  USING (upid)
LEFT JOIN ANCESTOR_SLICE(slice.id) binder_reply ON binder_reply.name = 'binder reply'
LEFT JOIN thread_track binder_reply_thread_track ON binder_reply.track_id = binder_reply_thread_track.id
LEFT JOIN thread binder_reply_thread ON binder_reply_thread_track.utid = binder_reply_thread.utid
JOIN internal_valid_android_monitor_contention ON internal_valid_android_monitor_contention.id = slice.id
JOIN thread blocking_thread ON blocking_thread.tid = blocking_tid AND blocking_thread.upid = thread.upid
WHERE slice.name GLOB 'monitor contention*'
  AND slice.dur != -1
  AND short_blocking_method IS NOT NULL
  AND short_blocked_method IS NOT NULL
GROUP BY slice.id;

CREATE INDEX internal_android_monitor_contention_idx
  ON android_monitor_contention (blocking_utid, ts);

-- Monitor contention slices that are blocked by another monitor contention slice.
-- They will have a |parent_id| field which is the id of the slice they are blocked by.
CREATE PERFETTO TABLE internal_children AS
SELECT parent.id AS parent_id, child.* FROM android_monitor_contention child
JOIN android_monitor_contention parent ON parent.blocked_utid = child.blocking_utid
AND child.ts BETWEEN parent.ts AND parent.ts + parent.dur;

-- Monitor contention slices that are blocking another monitor contention slice.
-- They will have a |child_id| field which is the id of the slice they are blocking.
CREATE PERFETTO TABLE internal_parents AS
SELECT parent.*, child.id AS child_id FROM android_monitor_contention parent
JOIN android_monitor_contention child ON parent.blocked_utid = child.blocking_utid
AND child.ts BETWEEN parent.ts AND parent.ts + parent.dur;

-- Monitor contention slices that are neither blocking nor blocked by another monitor contention
-- slice. They neither have |parent_id| nor |child_id| fields.
CREATE TABLE internal_isolated AS
WITH parents_and_children AS (
 SELECT id FROM internal_children
 UNION ALL
 SELECT id FROM internal_parents
), isolated AS (
    SELECT id FROM android_monitor_contention
    EXCEPT
    SELECT id FROM parents_and_children
  )
SELECT * FROM android_monitor_contention JOIN isolated USING (id);

-- Contains parsed monitor contention slices with the parent-child relationships.
--
-- @column parent_id Id of monitor contention slice blocking this contention.
-- @column blocking_method Name of the method holding the lock.
-- @column blocked_methhod Name of the method trying to acquire the lock.
-- @column short_blocking_method Blocking_method without arguments and return types.
-- @column short_blocked_method Blocked_method without arguments and return types.
-- @column blocking_src File location of blocking_method in form <filename:linenumber>.
-- @column blocked_src File location of blocked_method in form <filename:linenumber>.
-- @column waiter_count Zero indexed number of threads trying to acquire the lock.
-- @column blocking_utid Utid of thread holding the lock.
-- @column blocking_thread_name Thread name of thread holding the lock.
-- @column upid Upid of process experiencing lock contention.
-- @column process_name Process name of process experiencing lock contention.
-- @column id Slice id of lock contention.
-- @column ts Timestamp of lock contention start.
-- @column dur Duration of lock contention.
-- @column track_id Thread track id of blocked thread.
-- @column is_blocked_main_thread Whether the blocked thread is the main thread.
-- @column is_blocking_main_thread Whether the blocking thread is the main thread.
-- @column binder_reply_id Slice id of binder reply slice if lock contention was part of a binder txn.
-- @column binder_reply_ts Timestamp of binder reply slice if lock contention was part of a binder txn.
-- @column binder_reply_tid Tid of binder reply slice if lock contention was part of a binder txn.
-- @column child_id Id of monitor contention slice blocked by this contention.
CREATE TABLE android_monitor_contention_chain AS
SELECT NULL AS parent_id, *, NULL AS child_id FROM internal_isolated
UNION ALL
SELECT c.*, p.child_id FROM internal_children c
LEFT JOIN internal_parents p USING(id)
UNION
SELECT c.parent_id, p.* FROM internal_parents p
LEFT JOIN internal_children c USING(id);

CREATE INDEX internal_android_monitor_contention_chain_idx
  ON android_monitor_contention_chain (blocking_method, blocking_utid, ts);

-- First blocked node on a lock, i.e nodes with |waiter_count| = 0. The |dur| here is adjusted
-- to only account for the time between the first thread waiting and the first thread to acquire
-- the lock. That way, the thread state span joins below only compute the thread states where
-- the blocking thread is actually holding the lock. This avoids counting the time when another
-- waiter acquired the lock before the first waiter.
CREATE VIEW internal_first_blocked_contention
  AS
SELECT start.id, start.blocking_utid, start.ts, MIN(end.ts + end.dur) - start.ts AS dur
FROM android_monitor_contention_chain start
JOIN android_monitor_contention_chain end
  ON
    start.blocking_utid = end.blocking_utid
    AND start.blocking_method = end.blocking_method
    AND end.ts BETWEEN start.ts AND start.ts + start.dur
WHERE start.waiter_count = 0
GROUP BY start.id;

CREATE VIEW internal_blocking_thread_state
AS
SELECT utid AS blocking_utid, ts, dur, state, blocked_function
FROM thread_state;

-- Contains the span join of the first waiters in the |android_monitor_contention_chain| with their
-- blocking_thread thread state.

-- Note that we only span join the duration where the lock was actually held and contended.
-- This can be less than the duration the lock was 'waited on' when a different waiter acquired the
-- lock earlier than the first waiter.
--
-- @column parent_id Id of slice blocking the blocking_thread.
-- @column blocking_method Name of the method holding the lock.
-- @column blocked_methhod Name of the method trying to acquire the lock.
-- @column short_blocking_method Blocking_method without arguments and return types.
-- @column short_blocked_method Blocked_method without arguments and return types.
-- @column blocking_src File location of blocking_method in form <filename:linenumber>.
-- @column blocked_src File location of blocked_method in form <filename:linenumber>.
-- @column waiter_count Zero indexed number of threads trying to acquire the lock.
-- @column blocking_utid Utid of thread holding the lock.
-- @column blocking_thread_name Thread name of thread holding the lock.
-- @column upid Upid of process experiencing lock contention.
-- @column process_name Process name of process experiencing lock contention.
-- @column id Slice id of lock contention.
-- @column ts Timestamp of lock contention start.
-- @column dur Duration of lock contention.
-- @column track_id Thread track id of blocked thread.
-- @column is_blocked_main_thread Whether the blocked thread is the main thread.
-- @column is_blocking_main_thread Whether the blocking thread is the main thread.
-- @column binder_reply_id Slice id of binder reply slice if lock contention was part of a binder txn.
-- @column binder_reply_ts Timestamp of binder reply slice if lock contention was part of a binder txn.
-- @column binder_reply_tid Tid of binder reply slice if lock contention was part of a binder txn.
-- @column blocking_utid Utid of the blocking |thread_state|.
-- @column ts Timestamp of the blocking |thread_state|.
-- @column state Thread state of the blocking thread.
-- @column blocked_function Blocked kernel function of the blocking thread.
CREATE VIRTUAL TABLE android_monitor_contention_chain_thread_state
USING
  SPAN_JOIN(internal_first_blocked_contention PARTITIONED blocking_utid,
            internal_blocking_thread_state PARTITIONED blocking_utid);

-- Aggregated thread_states on the 'blocking thread', the thread holding the lock.
-- This builds on the data from |android_monitor_contention_chain| and
-- for each contention slice, it returns the aggregated sum of all the thread states on the
-- blocking thread.
--
-- Note that this data is only available for the first waiter on a lock.
--
-- @column id Slice id of the monitor contention.
-- @column thread_state A |thread_state| that occurred in the blocking thread during the contention.
-- @column thread_state_dur Total time the blocking thread spent in the |thread_state| during
-- contention.
-- @column thread_state_count Count of all times the blocking thread entered |thread_state| during
-- the contention.
CREATE VIEW android_monitor_contention_chain_thread_state_by_txn
AS
SELECT
  id,
  state AS thread_state,
  SUM(dur) AS thread_state_dur,
  COUNT(dur) AS thread_state_count
FROM android_monitor_contention_chain_thread_state
GROUP BY id, thread_state;

-- Aggregated blocked_functions on the 'blocking thread', the thread holding the lock.
-- This builds on the data from |android_monitor_contention_chain| and
-- for each contention, it returns the aggregated sum of all the kernel
-- blocked function durations on the blocking thread.
--
-- Note that this data is only available for the first waiter on a lock.
--
-- @column id Slice id of the monitor contention.
-- @column blocked_function Blocked kernel function in a thread state in the blocking thread during
-- the contention.
-- @column blocked_function_dur Total time the blocking thread spent in the |blocked_function|
-- during the contention.
-- @column blocked_function_count Count of all times the blocking thread executed the
-- |blocked_function| during the contention.
CREATE VIEW android_monitor_contention_chain_blocked_functions_by_txn
AS
SELECT
  id,
  blocked_function,
  SUM(dur) AS blocked_function_dur,
  COUNT(dur) AS blocked_function_count
FROM android_monitor_contention_chain_thread_state
WHERE blocked_function IS NOT NULL
GROUP BY id, blocked_function;

-- Returns a DAG of all Java lock contentions in a process.
-- Each node in the graph is a <thread:Java method> pair.
-- Each edge connects from a node waiting on a lock to a node holding a lock.
-- The weights of each node represent the cumulative wall time the node blocked
-- other nodes connected to it.
--
-- @arg upid INT         Upid of process to generate a lock graph for.
-- @column pprof BYTES   Pprof of lock graph.
CREATE PERFETTO FUNCTION android_monitor_contention_graph(upid INT)
RETURNS TABLE(pprof BYTES) AS
WITH contention_chain AS (
SELECT *,
       IIF(blocked_thread_name GLOB 'binder:*', 'binder', blocked_thread_name)
        AS blocked_thread_name_norm,
       IIF(blocking_thread_name GLOB 'binder:*', 'binder', blocking_thread_name)
        AS blocking_thread_name_norm
FROM android_monitor_contention_chain WHERE upid = $upid
GROUP BY id, parent_id
), graph AS (
SELECT
  id,
  dur,
  CAT_STACKS(blocked_thread_name_norm || ':' || short_blocked_method,
    blocking_thread_name_norm || ':' || short_blocking_method) AS stack
FROM contention_chain
WHERE parent_id IS NULL
UNION ALL
SELECT
c.id,
c.dur AS dur,
  CAT_STACKS(blocked_thread_name_norm || ':' || short_blocked_method,
             blocking_thread_name_norm || ':' || short_blocking_method, stack) AS stack
FROM contention_chain c, graph AS p
WHERE p.id = c.parent_id
) SELECT EXPERIMENTAL_PROFILE(stack, 'duration', 'ns', dur) AS pprof
  FROM graph;
