-- Copyright 2026 The Cobalt Authors. All Rights Reserved.
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
-- Perfetto trace_processor queries for Cobalt storage traces recorded by
-- profile_storage_perfetto.py (b/586773736).
--
-- Each query is one statement that starts with a "-- Q<n>: <title>" line.
-- trace_processor_shell -q accepts only one statement that returns rows per
-- file, so profile_storage_perfetto.py runs each block separately and writes
-- <stem>.q<n>.csv. To run a query by hand, paste its block into the Query
-- (SQL) page of https://ui.perfetto.dev.
--
-- Q7 and Q8 select the app process by name or as the owner of the
-- CrBrowserMain thread; profile_storage_perfetto.py replaces
-- 'com.google.android.youtube.tv' with --package. Timestamps use the trace
-- clock (CLOCK_BOOTTIME), the same clock as /proc/uptime.

-- Q1: Storage I/O by subsystem and operation (excluding /proc/self/smaps and File lifetime spans)
SELECT
  CASE
    WHEN s.category = 'sql' AND EXTRACT_ARG(s.arg_set_id, 'debug.statement') GLOB '* cookies*' THEN 'USER_DATA: SQLite Cookies'
    WHEN s.category = 'sql' AND (EXTRACT_ARG(s.arg_set_id, 'debug.statement') GLOB '* bounces*' OR EXTRACT_ARG(s.arg_set_id, 'debug.statement') GLOB '* popups*') THEN 'USER_DATA: SQLite DIPS'
    WHEN s.category = 'sql' AND EXTRACT_ARG(s.arg_set_id, 'debug.statement') GLOB '* buckets*' THEN 'USER_DATA: SQLite QuotaManager'
    WHEN s.category = 'sql' AND EXTRACT_ARG(s.arg_set_id, 'debug.statement') GLOB '* dictionaries*' THEN 'USER_DATA: SQLite Shared Dictionary'
    WHEN s.category = 'sql' THEN 'USER_DATA: SQLite (open / commit / unresolved DB)'
    WHEN s.category = 'leveldb' THEN 'USER_DATA: LevelDB (Sync / SyncParent)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Local Storage/leveldb*' THEN 'USER_DATA: Local Storage (leveldb)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Session Storage*' THEN 'USER_DATA: Session Storage (leveldb)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Service Worker*' THEN 'USER_DATA: Service Worker (leveldb)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Shared Dictionary*' THEN 'USER_DATA: Shared Dictionary'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/WebStorage*' THEN 'USER_DATA: QuotaManager (WebStorage)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/shared_proto_db*' THEN 'USER_DATA: shared_proto_db (leveldb)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Cache/Cache_Data/*' THEN 'CACHE: SimpleCache (HTTP Cache_Data)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Code Cache/js/*' THEN 'CACHE: V8 Code Cache (js)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/GrShaderCache*' THEN 'CACHE: GrShaderCache'
    -- The ParkableImage swap file is opened by path once, then written with
    -- positional File::Write calls that carry an empty path.
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/cache/.org.chromium.Chromium*' OR (EXTRACT_ARG(s.arg_set_id, 'debug.path') = '' AND s.name = 'File::Write') THEN 'CACHE: ParkableImage swap (cache/.org.chromium.Chromium.*)'
    WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') = '' THEN 'USER_DATA: ImportantFileWriter (temp_file)'
    ELSE COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.path'), s.category)
  END AS storage_subsystem,
  s.category,
  s.name AS operation,
  COUNT(*) AS ops,
  ROUND(SUM(COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.size'), 0)) / 1024.0, 1) AS total_kb,
  ROUND(SUM(s.dur) / 1e6, 2) AS total_ms,
  ROUND(MAX(s.dur) / 1e6, 2) AS max_ms
FROM slice s
WHERE s.category IN ('sql', 'leveldb', 'disabled-by-default-file')
  AND s.name != 'File'
  AND COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.path'), '') NOT GLOB '*/proc/self/smaps*'
GROUP BY storage_subsystem, s.category, operation
ORDER BY total_ms DESC;

-- Q2: Top 30 slowest storage operations with calling thread, file path and SQL statement
SELECT
  s.id,
  ROUND((s.ts - (SELECT start_ts FROM trace_bounds)) / 1e9, 3) AS time_sec,
  COALESCE(th1.name, th2.name, 'Async') AS calling_thread,
  s.category,
  s.name,
  ROUND(s.dur / 1e6, 2) AS dur_ms,
  COALESCE(NULLIF(EXTRACT_ARG(s.arg_set_id, 'debug.path'), ''), EXTRACT_ARG(s.arg_set_id, 'debug.statement'), '') AS path_or_sql,
  COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.size'), 0) AS bytes
FROM slice s
LEFT JOIN thread_track tt ON s.track_id = tt.id
LEFT JOIN thread th1 ON tt.utid = th1.utid
LEFT JOIN thread th2 ON EXTRACT_ARG(s.arg_set_id, 'legacy_event.passthrough_utid') = th2.utid
WHERE s.category IN ('sql', 'leveldb', 'disabled-by-default-file')
  AND s.name != 'File'
  AND COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.path'), '') NOT GLOB '*/proc/self/smaps*'
ORDER BY s.dur DESC
LIMIT 30;

-- Q3: Storage UMA histogram samples (aggregated)
SELECT
  EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') AS histogram_name,
  COUNT(*) AS count,
  MIN(EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.sample')) AS min_val,
  ROUND(AVG(EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.sample')), 2) AS avg_val,
  MAX(EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.sample')) AS max_val,
  SUM(EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.sample')) AS sum_val,
  GROUP_CONCAT(CAST(EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.sample') AS TEXT), ', ') AS samples
FROM slice s
WHERE s.name = 'HistogramSample'
  AND (
    EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Cobalt.Storage*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Cobalt.LocalStorage*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'SimpleCache.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'HttpCache.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'V8.Compile*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'DOMStorage.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'LocalStorage.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'SessionStorageContext.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'LevelDB*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Cookie.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Sql.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Memory.Parkable*'
  )
GROUP BY histogram_name
ORDER BY histogram_name;

-- Q4: Storage UMA histogram samples (chronological)
SELECT
  s.id,
  ROUND((s.ts - (SELECT start_ts FROM trace_bounds)) / 1e9, 3) AS time_sec,
  COALESCE(th.name, 'Async') AS calling_thread,
  EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') AS histogram_name,
  EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.sample') AS sample_value
FROM slice s
LEFT JOIN thread_track tt ON s.track_id = tt.id
LEFT JOIN thread th ON tt.utid = th.utid
WHERE s.name = 'HistogramSample'
  AND (
    EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Cobalt.Storage*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'DOMStorage.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'SimpleCache.*.CacheSizeOnInit'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'SimpleCache.*.MaxCacheSizeOnInit'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'SimpleCache.Code.Eviction.*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Cookie.Time*'
    OR EXTRACT_ARG(s.arg_set_id, 'chrome_histogram_sample.name') GLOB 'Sql.Database.Success.OpenInternalTime.*'
  )
ORDER BY s.ts;

-- Q5: Storage engine first/last touch (offsets from the first storage slice, plus CLOCK_BOOTTIME seconds to compare with <stem>.phases.json)
WITH StorageOps AS (
  SELECT
    s.ts,
    s.dur,
    CASE
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Local Storage/*' THEN 'LocalStorage'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Code Cache/*' THEN 'CodeCache'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Service Worker*' OR s.category = 'ServiceWorker' THEN 'ServiceWorker'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Shared Dictionary*' THEN 'SharedDictionary'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/DIPS*' THEN 'DIPS'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Cookies*' THEN 'Cookies'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Cache/Cache_Data/*' THEN 'HttpCache'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Session Storage/*' THEN 'SessionStorage'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/WebStorage*' THEN 'QuotaManager'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/GrShaderCache*' THEN 'GrShaderCache'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/shared_proto_db*' THEN 'shared_proto_db'
      WHEN s.category = 'leveldb' THEN 'LevelDB::Sync'
      WHEN s.category = 'sql' THEN 'SQLite::Query'
      ELSE NULL
    END AS engine
  FROM slice s
  WHERE s.category IN ('sql', 'leveldb', 'ServiceWorker', 'disabled-by-default-file')
    AND s.name != 'File'
),
Origin AS (
  SELECT MIN(ts) AS t0 FROM StorageOps WHERE engine IS NOT NULL
)
SELECT
  engine,
  ROUND((MIN(ts) - (SELECT t0 FROM Origin)) / 1e6, 1) AS first_offset_ms,
  ROUND((MAX(ts + dur) - (SELECT t0 FROM Origin)) / 1e6, 1) AS last_end_ms,
  COUNT(*) AS ops,
  ROUND(SUM(dur) / 1e6, 2) AS total_dur_ms,
  ROUND(MIN(ts) / 1e9, 3) AS first_boottime_s,
  ROUND(MAX(ts + dur) / 1e9, 3) AS last_end_boottime_s
FROM StorageOps
WHERE engine IS NOT NULL
GROUP BY engine
ORDER BY first_offset_ms ASC;

-- Q6: Cross-thread eMMC contention: overlapping >= 5 ms storage slices of two different engines (each engine pair counted once)
WITH EngineSlices AS (
  SELECT
    s.id,
    s.track_id,
    s.ts,
    s.ts + s.dur AS end_ts,
    s.dur,
    CASE
      WHEN s.category = 'leveldb' THEN 'LevelDB::Sync'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Cookies*' THEN 'Cookies'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Local Storage/*' THEN 'LocalStorage'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/DIPS*' THEN 'DIPS'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Service Worker*' THEN 'ServiceWorker'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Session Storage/*' THEN 'SessionStorage'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/WebStorage*' THEN 'QuotaManager'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Cache/Cache_Data/*' THEN 'HttpCache'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/Code Cache/*' THEN 'CodeCache'
      WHEN EXTRACT_ARG(s.arg_set_id, 'debug.path') GLOB '*/GrShaderCache*' THEN 'GrShaderCache'
      WHEN s.category = 'sql' THEN 'SQLite'
      ELSE 'OtherFileIO'
    END AS engine,
    COALESCE(NULLIF(EXTRACT_ARG(s.arg_set_id, 'debug.path'), ''), s.name) AS detail
  FROM slice s
  WHERE s.category IN ('sql', 'leveldb', 'disabled-by-default-file')
    AND s.name != 'File'
    AND s.dur >= 5000000
    AND COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.path'), '') NOT GLOB '*/proc/self/smaps*'
)
SELECT
  a.engine AS engine_a,
  b.engine AS engine_b,
  COUNT(*) AS overlap_pairs,
  ROUND(SUM(MIN(a.end_ts, b.end_ts) - MAX(a.ts, b.ts)) / 1e6, 2) AS overlap_ms,
  MAX(a.detail) AS sample_detail_a,
  MAX(b.detail) AS sample_detail_b
FROM EngineSlices a
JOIN EngineSlices b
  ON a.engine < b.engine
  AND a.track_id != b.track_id
  AND a.ts < b.end_ts
  AND b.ts < a.end_ts
GROUP BY engine_a, engine_b
ORDER BY overlap_ms DESC
LIMIT 25;

-- Q7: Kernel block I/O wait (thread_state state = D and io_wait = 1) of the app by thread and kernel function
SELECT
  p.name AS process_name,
  -- ftrace and /proc thread names are truncated to 15 characters.
  CASE
    WHEN t.name GLOB 'ThreadPoolForeg*' THEN 'ThreadPoolForegroundWorker'
    WHEN t.name GLOB 'ThreadPoolBackg*' THEN 'ThreadPoolBackgroundWorker'
    WHEN t.name GLOB 'ThreadPoolServi*' THEN 'ThreadPoolServiceThread'
    WHEN t.name GLOB 'Chrome_InProcRe*' THEN 'Chrome_InProcRendererThread'
    WHEN t.name GLOB 'Chrome_InProcGp*' THEN 'Chrome_InProcGpuThread'
    WHEN t.name GLOB 'VizCompositorTh*' THEN 'VizCompositorThread'
    WHEN t.name GLOB 'CompositorTileW*' THEN 'CompositorTileWorker'
    ELSE t.name
  END AS thread_name,
  COALESCE(ts.blocked_function, '') AS blocked_function,
  COUNT(*) AS io_wait_spans,
  ROUND(SUM(ts.dur) / 1e6, 2) AS total_io_wait_ms,
  ROUND(MAX(ts.dur) / 1e6, 2) AS max_io_wait_ms
FROM thread_state ts
JOIN thread t USING (utid)
JOIN process p USING (upid)
-- An app forked from zygote during the trace can keep the name
-- "<pre-initialized>", so also match the process that owns CrBrowserMain.
WHERE (p.name = 'com.google.android.youtube.tv' OR p.upid IN (SELECT upid FROM thread WHERE name = 'CrBrowserMain'))
  AND ts.state = 'D'
  AND ts.io_wait = 1
  AND ts.dur > 0
GROUP BY process_name, thread_name, blocked_function
ORDER BY total_io_wait_ms DESC
LIMIT 30;

-- Q8: UI/Renderer/GPU/Compositor kernel io_wait overlapped by concurrent storage blockers (one row per victim thread and blocker category)
-- overlap_ms is the part of the victim's io_wait time during which at least
-- one blocker of the category was active (blockers that overlap each other
-- are not double counted). top_blocker is the "name / thread" label with the
-- most overlap.
WITH
-- Same 15-character thread name normalization as Q7.
thread_norm AS (
  SELECT
    t.utid,
    t.upid,
    CASE
      WHEN t.name GLOB 'ThreadPoolForeg*' THEN 'ThreadPoolForegroundWorker'
      WHEN t.name GLOB 'ThreadPoolBackg*' THEN 'ThreadPoolBackgroundWorker'
      WHEN t.name GLOB 'ThreadPoolServi*' THEN 'ThreadPoolServiceThread'
      WHEN t.name GLOB 'Chrome_InProcRe*' THEN 'Chrome_InProcRendererThread'
      WHEN t.name GLOB 'Chrome_InProcGp*' THEN 'Chrome_InProcGpuThread'
      WHEN t.name GLOB 'VizCompositorTh*' THEN 'VizCompositorThread'
      WHEN t.name GLOB 'CompositorTileW*' THEN 'CompositorTileWorker'
      ELSE COALESCE(t.name, 'tid ' || t.tid)
    END AS name
  FROM thread t
),
victim AS (
  SELECT ts.id AS span_id, ts.utid, ts.ts, ts.ts + ts.dur AS end_ts, ts.dur, tn.name AS victim_thread
  FROM thread_state ts
  JOIN thread_norm tn USING (utid)
  JOIN process p USING (upid)
  -- Same app process match as Q7.
  WHERE (p.name = 'com.google.android.youtube.tv' OR p.upid IN (SELECT upid FROM thread WHERE name = 'CrBrowserMain'))
    AND ts.state = 'D'
    AND ts.io_wait = 1
    AND ts.dur > 0
    AND tn.name IN ('CrBrowserMain', 'Chrome_InProcRendererThread', 'Chrome_InProcGpuThread', 'VizCompositorThread', 'Compositor')
),
victim_total AS (
  SELECT victim_thread, SUM(dur) AS io_wait_ns FROM victim GROUP BY victim_thread
),
worker_slice AS (
  SELECT
    s.ts,
    s.ts + s.dur AS end_ts,
    COALESCE(tt.utid, EXTRACT_ARG(s.arg_set_id, 'legacy_event.passthrough_utid')) AS utid,
    s.name || COALESCE(':' || EXTRACT_ARG(s.arg_set_id, 'source.function_name'), '') AS name
  FROM slice s
  LEFT JOIN thread_track tt ON s.track_id = tt.id
  WHERE s.dur > 0
    AND s.name != 'File'
    AND COALESCE(EXTRACT_ARG(s.arg_set_id, 'debug.path'), '') NOT GLOB '/proc/*'
    AND (
      s.category IN ('sql', 'leveldb', 'disabled-by-default-file')
      -- Only file, SQLite and LevelDB blocking calls (not DNS, certificate
      -- verification or /proc/self/smaps scans).
      OR (s.category = 'base' AND s.name = 'ScopedBlockingCall' AND (
        EXTRACT_ARG(s.arg_set_id, 'source.file_name') GLOB 'base/files/*'
        OR EXTRACT_ARG(s.arg_set_id, 'source.file_name') GLOB 'sql/*'
        OR EXTRACT_ARG(s.arg_set_id, 'source.file_name') GLOB 'third_party/leveldatabase/*'))
    )
),
fsync_event AS (
  SELECT
    f.ts,
    f.name,
    f.utid,
    LEAD(f.ts) OVER (PARTITION BY f.utid ORDER BY f.ts) AS next_ts,
    LEAD(f.name) OVER (PARTITION BY f.utid ORDER BY f.ts) AS next_name
  FROM ftrace_event f
  WHERE f.name IN ('ext4_sync_file_enter', 'ext4_sync_file_exit', 'f2fs_sync_file_enter', 'f2fs_sync_file_exit')
),
block_event AS (
  SELECT
    e.*,
    LEAD(e.ts) OVER (PARTITION BY e.dev, e.sector ORDER BY e.ts) AS next_ts,
    LEAD(e.name) OVER (PARTITION BY e.dev, e.sector ORDER BY e.ts) AS next_name
  FROM (
    SELECT
      f.ts,
      f.name,
      f.utid,
      EXTRACT_ARG(f.arg_set_id, 'dev') AS dev,
      EXTRACT_ARG(f.arg_set_id, 'sector') AS sector,
      EXTRACT_ARG(f.arg_set_id, 'rwbs') AS rwbs
    FROM ftrace_event f
    WHERE f.name IN ('block_rq_issue', 'block_rq_complete')
  ) e
),
blocker AS (
  SELECT 'worker_storage_slice' AS category, w.ts, w.end_ts, w.utid, w.name || ' / ' || COALESCE(tn.name, '?') AS label
  FROM worker_slice w
  LEFT JOIN thread_norm tn USING (utid)
  -- MemoryInfra reads /proc files through base/files ScopedBlockingCalls.
  WHERE NOT (w.name GLOB 'ScopedBlockingCall*' AND tn.name IS 'MemoryInfra')
  UNION ALL
  SELECT 'fsync', e.ts, e.next_ts, e.utid, REPLACE(e.name, '_enter', '') || ' / ' || COALESCE(tn.name, '?')
  FROM fsync_event e
  LEFT JOIN thread_norm tn USING (utid)
  WHERE e.name GLOB '*_enter' AND e.next_name = REPLACE(e.name, '_enter', '_exit')
  UNION ALL
  SELECT 'block_rq', e.ts, e.next_ts, e.utid, 'block_rq ' || COALESCE(e.rwbs, '?') || ' / ' || COALESCE(tn.name, '?')
  FROM block_event e
  LEFT JOIN thread_norm tn USING (utid)
  WHERE e.name = 'block_rq_issue' AND e.next_name = 'block_rq_complete'
),
overlap AS (
  SELECT
    v.span_id,
    v.victim_thread,
    b.category,
    b.label,
    MAX(v.ts, b.ts) AS o_start,
    MIN(v.end_ts, b.end_ts) AS o_end
  FROM victim v
  JOIN blocker b ON b.ts < v.end_ts AND b.end_ts > v.ts
  -- A victim's own storage slices are not "worker" blockers.
  WHERE NOT (b.category = 'worker_storage_slice' AND b.utid IS v.utid)
),
-- Sweep each victim span's overlaps in start order: an overlap adds only the
-- part that ends after every earlier overlap of the same category (or label).
covered AS (
  SELECT
    victim_thread,
    category,
    label,
    o_start,
    o_end,
    MAX(o_end) OVER (PARTITION BY span_id, category ORDER BY o_start, o_end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) AS prev_end,
    MAX(o_end) OVER (PARTITION BY span_id, category, label ORDER BY o_start, o_end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) AS prev_label_end
  FROM overlap
),
category_total AS (
  SELECT victim_thread, category, SUM(MAX(0, o_end - MAX(o_start, COALESCE(prev_end, o_start)))) AS overlap_ns
  FROM covered
  GROUP BY victim_thread, category
),
label_total AS (
  SELECT victim_thread, category, label, SUM(MAX(0, o_end - MAX(o_start, COALESCE(prev_label_end, o_start)))) AS label_ns
  FROM covered
  GROUP BY victim_thread, category, label
),
top_label AS (
  SELECT *, ROW_NUMBER() OVER (PARTITION BY victim_thread, category ORDER BY label_ns DESC) AS rn
  FROM label_total
)
SELECT
  c.victim_thread,
  c.category AS blocker_category,
  ROUND(vt.io_wait_ns / 1e6, 2) AS victim_io_wait_ms,
  ROUND(c.overlap_ns / 1e6, 2) AS overlap_ms,
  ROUND(100.0 * c.overlap_ns / vt.io_wait_ns, 1) AS overlap_pct,
  l.label AS top_blocker,
  ROUND(l.label_ns / 1e6, 2) AS top_blocker_ms
FROM category_total c
JOIN victim_total vt USING (victim_thread)
JOIN top_label l ON l.victim_thread = c.victim_thread AND l.category = c.category AND l.rn = 1
ORDER BY vt.io_wait_ns DESC, c.overlap_ns DESC;
