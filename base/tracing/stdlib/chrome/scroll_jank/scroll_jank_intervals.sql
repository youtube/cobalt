-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.chrome_scrolls;
INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;
INCLUDE PERFETTO MODULE common.slices;

-- Selects EventLatency slices that correspond with janks in a scroll. This is
-- based on the V3 version of scroll jank metrics.
--
-- @column id INT                     The slice id.
-- @column ts INT                     The start timestamp of the slice.
-- @column dur INT                    The duration of the slice.
-- @column track_id INT               The track_id for the slice.
-- @column name STRING                The name of the slice (EventLatency).
-- @column cause_of_jank STRING       The stage of EventLatency that the caused
--                                    the jank.
-- @column sub_cause_of_jank STRING   The stage of cause_of_jank that caused the
--                                    jank.
-- @column delayed_frame_count INT    How many vsyncs this frame missed its
--                                    deadline by.
-- @column frame_jank_ts INT          The start timestamp where frame
--                                    frame presentation was delayed.
-- @column frame_jank_dur INT         The duration in ms of the delay in frame
--                                    presentation.
CREATE PERFETTO TABLE chrome_janky_event_latencies_v3 AS
SELECT
  s.id,
  s.ts,
  s.dur,
  s.track_id,
  s.name,
  e.cause_of_jank,
  e.sub_cause_of_jank,
  CAST((e.delay_since_last_frame/e.vsync_interval) - 1 AS INT) AS delayed_frame_count,
  CAST(s.ts + s.dur - ((e.delay_since_last_frame - e.vsync_interval) * 1e6) AS INT) AS frame_jank_ts,
  CAST((e.delay_since_last_frame - e.vsync_interval) * 1e6 AS INT) AS frame_jank_dur
FROM slice s
JOIN chrome_janky_frames e
  ON s.id = e. event_latency_id;

-- Frame presentation interval is the delta between when the frame was supposed
-- to be presented and when it was actually presented.
--
-- @column id INT                     Unique id.
-- @column ts INT                     The start timestamp of the slice.
-- @column dur INT                    The duration of the slice.
-- @column delayed_frame_count INT    How many vsyncs this frame missed its
--                                    deadline by.
-- @column cause_of_jank STRING       The stage of EventLatency that the caused
--                                    the jank.
-- @column sub_cause_of_jank STRING   The stage of cause_of_jank that caused the
--                                    jank.
-- @column event_latency_id STRING    The id of the associated event latency in
--                                    the slice table.
CREATE VIEW chrome_janky_frame_presentation_intervals AS
SELECT
  ROW_NUMBER() OVER(ORDER BY frame_jank_ts) AS id,
  frame_jank_ts AS ts,
  frame_jank_dur AS dur,
  delayed_frame_count,
  cause_of_jank,
  sub_cause_of_jank,
  id AS event_latency_id
FROM chrome_janky_event_latencies_v3;

-- Scroll jank frame presentation stats for individual scrolls.
--
-- @column scroll_id INT              Id of the individual scroll.
-- @column missed_vsyncs INT          The number of missed vsyncs in the scroll.
-- @column frame_count INT            The number of frames in the scroll.
-- @column presented_frame_count INT  The number presented frames in the scroll.
-- @column janky_frame_count INT      The number of janky frames in the scroll.
-- @column janky_frame_percent FLOAT  The % of frames that janked in the scroll.
CREATE VIEW chrome_scroll_stats AS
WITH vsyncs AS (
  SELECT
    COUNT() AS presented_vsync_count,
    scroll.id AS scroll_id
  FROM chrome_unique_frame_presentation_ts frame
  JOIN chrome_scrolls scroll
    ON frame.presentation_timestamp >= scroll.ts
    AND frame.presentation_timestamp <= scroll.ts + scroll.dur
  GROUP BY scroll_id),
missed_vsyncs AS (
  SELECT
    CAST(SUM((delay_since_last_frame / vsync_interval) - 1) AS INT)  AS total_missed_vsyncs,
    scroll_id
  FROM chrome_janky_frames
  GROUP BY scroll_id),
frame_stats AS (
  SELECT
    scroll_id,
    num_frames AS presented_frame_count,
    IFNULL(num_janky_frames, 0) AS janky_frame_count,
    ROUND(IFNULL(scroll_jank_percentage, 0), 2) AS janky_frame_percent
  FROM chrome_frames_per_scroll
)
SELECT
  vsyncs.scroll_id,
  presented_vsync_count + IFNULL(total_missed_vsyncs, 0) AS frame_count,
  total_missed_vsyncs AS missed_vsyncs,
  presented_frame_count,
  janky_frame_count,
  janky_frame_percent
FROM vsyncs
LEFT JOIN missed_vsyncs
  USING (scroll_id)
LEFT JOIN frame_stats
  USING (scroll_id);

-- Defines slices for all of janky scrolling intervals in a trace.
--
-- @column id            The unique identifier of the janky interval.
-- @column ts            The start timestamp of the janky interval.
-- @column dur           The duration of the janky interval.
CREATE PERFETTO TABLE chrome_scroll_jank_intervals_v3 AS
-- Sub-table to retrieve all janky slice timestamps. Ordering calculations are
-- based on timestamps rather than durations.
WITH janky_latencies AS (
  SELECT
    s.frame_jank_ts AS start_ts,
    s.frame_jank_ts + s.frame_jank_dur AS end_ts
  FROM chrome_janky_event_latencies_v3 s),
-- Determine the local maximum timestamp for janks thus far; this will allow
-- us to coalesce all earlier events up to the maximum.
ordered_jank_end_ts AS (
  SELECT
    *,
    MAX(end_ts) OVER (
      ORDER BY start_ts ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW)
      AS max_end_ts_so_far
  FROM janky_latencies),
-- Determine the local minimum timestamp for janks thus far; this will allow
-- us to coalesce all later events up to the nearest local maximum.
range_starts AS (
  SELECT
    *,
    CASE
      -- This is a two-pass calculation to calculate the first event in the
      -- group. An event is considered the first event in a group if all events
      -- which started before it also finished the current one started.
      WHEN start_ts <= 1 + LAG(max_end_ts_so_far) OVER (ORDER BY start_ts) THEN 0
      ELSE 1
    END AS range_start
  FROM ordered_jank_end_ts),
-- Assign an id to allow coalescing of individual slices.
range_groups AS (
  SELECT
    *,
    SUM(range_start) OVER (ORDER BY start_ts) AS range_group
  FROM range_starts)
-- Coalesce all slices within an interval.
SELECT
  range_group AS id,
  MIN(start_ts) AS ts,
  MAX(end_ts) - MIN(start_ts) AS dur
FROM range_groups
GROUP BY range_group;
