#!/usr/bin/env python3
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License a
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeScrollJank(TestSuite):
  # Scroll jank metrics
  def test_scroll_jank_general_validation(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query=Path('scroll_jank_general_validation_test.sql'),
        out=Path('scroll_jank_general_validation.out'))

  def test_scroll_jank(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank.sql');

        SELECT
          gesture_scroll_id,
          trace_id,
          jank,
          ts,
          dur,
          jank_budget
        FROM scroll_jank;
        """,
        out=Path('scroll_jank.out'))

  def test_event_latency_to_breakdowns(self):
    return DiffTestBlueprint(
        trace=DataPath('event_latency_with_args.perfetto-trace'),
        query="""
        SELECT RUN_METRIC('chrome/event_latency_to_breakdowns.sql');

        SELECT
          event_latency_ts,
          event_latency_dur,
          event_type,
          GenerationToRendererCompositorNs,
          GenerationToBrowserMainNs,
          BrowserMainToRendererCompositorNs,
          RendererCompositorQueueingDelayNs,
          unknown_stages_seen
        FROM event_latency_to_breakdowns
        ORDER BY event_latency_id
        LIMIT 30;
        """,
        out=Path('event_latency_to_breakdowns.out'))

  def test_event_latency_scroll_jank(self):
    return DiffTestBlueprint(
        trace=DataPath('event_latency_with_args.perfetto-trace'),
        query="""
        SELECT RUN_METRIC('chrome/event_latency_scroll_jank.sql');

        SELECT
          jank,
          next_jank,
          prev_jank,
          gesture_begin_ts,
          gesture_end_ts,
          ts,
          dur,
          event_type,
          next_ts,
          next_dur,
          prev_ts,
          prev_dur
        FROM scroll_event_latency_jank
        ORDER BY jank DESC
        LIMIT 10;
        """,
        out=Path('event_latency_scroll_jank.out'))

  def test_event_latency_scroll_jank_cause(self):
    return DiffTestBlueprint(
        trace=DataPath('event_latency_with_args.perfetto-trace'),
        query="""
        SELECT RUN_METRIC('chrome/event_latency_scroll_jank_cause.sql');

        SELECT
          dur,
          ts,
          event_type,
          next_jank,
          prev_jank,
          next_delta_dur_ns,
          prev_delta_dur_ns,
          cause_of_jank,
          max_delta_dur_ns,
          sub_cause_of_jank
        FROM event_latency_scroll_jank_cause
        ORDER by ts;
        """,
        out=Path('event_latency_scroll_jank_cause.out'))

  def test_scroll_flow_event(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_flow_event.sql');

        SELECT
          trace_id,
          ts,
          dur,
          jank,
          step,
          ancestor_end,
          maybe_next_ancestor_ts,
          next_ts,
          next_trace_id,
          next_step
        FROM scroll_flow_event
        ORDER BY gesture_scroll_id, trace_id, ts;
        """,
        out=Path('scroll_flow_event.out'))

  def test_scroll_flow_event_general_validation(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_flow_event.sql');

        SELECT
          -- Each trace_id (in our example trace not true in general) has 8 steps. There
          -- are 139 scrolls. So we expect 1112 rows in total 72 of which are janky.
          (
            SELECT
              COUNT(*)
            FROM (
              SELECT
                trace_id,
                COUNT(*)
              FROM scroll_flow_event
              GROUP BY trace_id
            )
          ) AS total_scroll_updates,
          (
            SELECT COUNT(*) FROM scroll_flow_event
          ) AS total_flow_event_steps,
          (
            SELECT COUNT(*) FROM scroll_flow_event WHERE jank
          ) AS total_janky_flow_event_steps,
          (
            SELECT COUNT(*) FROM (SELECT step FROM scroll_flow_event GROUP BY step)
          ) AS number_of_unique_steps;
        """,
        out=Path('scroll_flow_event_general_validation.out'))

  def test_scroll_jank_cause(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank_cause.sql');

        SELECT
          COUNT(*) AS total,
          SUM(jank) AS total_jank,
          SUM(explained_jank + unexplained_jank) AS sum_explained_and_unexplained,
          SUM(
            CASE WHEN explained_jank THEN
              unexplained_jank
              ELSE
                CASE WHEN jank AND NOT unexplained_jank THEN
                  1
                  ELSE
                    0
                END
            END
          ) AS error_rows
        FROM scroll_jank_cause;
        """,
        out=Csv("""
        "total","total_jank","sum_explained_and_unexplained","error_rows"
        139,7,7,0
        """))

  def test_scroll_flow_event_queuing_delay(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_flow_event_queuing_delay.sql');

        SELECT
          trace_id,
          jank,
          step,
          next_step,
          ancestor_end,
          maybe_next_ancestor_ts,
          queuing_time_ns
        FROM scroll_flow_event_queuing_delay
        WHERE trace_id = 2954 OR trace_id = 2956 OR trace_id = 2960
        ORDER BY trace_id, ts;
        """,
        out=Path('scroll_flow_event_queuing_delay.out'))

  def test_scroll_flow_event_general_validation_2(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query=Path(
            'scroll_flow_event_queuing_delay_general_validation_test.sql'),
        out=Path('scroll_flow_event_general_validation.out'))

  def test_scroll_jank_cause_queuing_delay(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank_cause_queuing_delay.sql');

        SELECT
          process_name,
          thread_name,
          trace_id,
          jank,
          dur_overlapping_ns,
          metric_name
        FROM scroll_jank_cause_queuing_delay
        WHERE trace_id = 2918 OR trace_id = 2926
        ORDER BY trace_id ASC, ts ASC;
        """,
        out=Path('scroll_jank_cause_queuing_delay.out'))

  def test_scroll_jank_cause_queuing_delay_restricted(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank_cause_queuing_delay.sql');

        SELECT
          process_name,
          thread_name,
          trace_id,
          jank,
          dur_overlapping_ns,
          restricted_metric_name
        FROM scroll_jank_cause_queuing_delay
        WHERE trace_id = 2918 OR trace_id = 2926
        ORDER BY trace_id ASC, ts ASC;
        """,
        out=Path('scroll_jank_cause_queuing_delay_restricted.out'))

  def test_scroll_jank_cause_queuing_delay_general_validation(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank_cause_queuing_delay.sql');

        SELECT
          COUNT(*) AS total,
          (
            SELECT DISTINCT
              (avg_no_jank_dur_overlapping_ns)
            FROM scroll_jank_cause_queuing_delay
            WHERE
              location = "LatencyInfo.Flow"
              AND jank
          ) AS janky_latency_info_non_jank_avg_dur,
          (
            SELECT DISTINCT
              (avg_no_jank_dur_overlapping_ns)
            FROM scroll_jank_cause_queuing_delay
            WHERE
              location = "LatencyInfo.Flow"
              AND NOT jank
          ) AS non_janky_latency_info_non_jank_avg_dur
        FROM (
          SELECT
            trace_id
          FROM scroll_jank_cause_queuing_delay
          GROUP BY trace_id
        );
        """,
        out=Path('scroll_jank_cause_queuing_delay_general_validation.out'))

  def test_chrome_thread_slice(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_scroll_without_vsync.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_thread_slice.sql');

        SELECT
          EXTRACT_ARG(arg_set_id, 'chrome_latency_info.trace_id') AS trace_id,
          dur,
          thread_dur
        FROM chrome_thread_slice
        WHERE
          name = 'LatencyInfo.Flow'
          AND EXTRACT_ARG(arg_set_id, 'chrome_latency_info.trace_id') = 2734;
        """,
        out=Csv("""
        "trace_id","dur","thread_dur"
        2734,25000,25000
        2734,1000,2000
        2734,2000,2000
        2734,258000,171000
        2734,1000,1000
        """))

  def test_chrome_input_to_browser_intervals(self):
    return DiffTestBlueprint(
        trace=DataPath('scrolling_with_blocked_nonblocked_frames.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_input_to_browser_intervals.sql');

        SELECT
          *
        FROM chrome_input_to_browser_intervals
        WHERE window_start_ts >= 60934320005158
          AND window_start_ts <= 60934338798158;
        """,
        out=Path('chrome_input_to_browser_intervals.out'))

  def test_chrome_scroll_jank_caused_by_scheduling(self):
    return DiffTestBlueprint(
        trace=DataPath('fling_with_input_delay.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_scroll_jank_caused_by_scheduling.sql',
          'dur_causes_jank_ms',
        /* dur_causes_jank_ms = */ '5');

        SELECT
          full_name,
          total_duration_ms,
          total_thread_duration_ms,
          count,
          window_start_ts,
          window_end_ts,
          scroll_type
        FROM chrome_scroll_jank_caused_by_scheduling;
        """,
        out=Path('chrome_scroll_jank_caused_by_scheduling_test.out'))

  def test_chrome_tasks_delaying_input_processing(self):
    return DiffTestBlueprint(
        trace=DataPath('fling_with_input_delay.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_tasks_delaying_input_processing.sql',
          'duration_causing_jank_ms',
         /* duration_causing_jank_ms = */ '8');

        SELECT
          full_name,
          duration_ms,
          thread_dur_ms
        FROM chrome_tasks_delaying_input_processing;
        """,
        out=Path('chrome_tasks_delaying_input_processing_test.out'))

  def test_long_task_tracking_trace_chrome_long_tasks_delaying_input_processing(
      self):
    return DiffTestBlueprint(
        trace=DataPath('long_task_tracking_trace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_long_tasks_delaying_input_processing.sql');

        SELECT
          full_name,
          duration_ms,
          slice_id
        FROM chrome_tasks_delaying_input_processing
        ORDER BY slice_id;
        """,
        out=Path(
            'long_task_tracking_trace_chrome_long_tasks_delaying_input_processing_test.out'
        ))

  # TODO(b/264520610): Uncomment once fixed
  # chrome_long_tasks_delaying_input_processing_compare_default_test.sql
  # long_task_tracking_trace_chrome_long_tasks_delaying_input_processing_compare_default_test.out
  def test_experimental_reliable_chrome_tasks_delaying_input_processing(self):
    return DiffTestBlueprint(
        trace=DataPath('fling_with_input_delay.pftrace'),
        query="""
        SELECT RUN_METRIC(
            'chrome/experimental_reliable_chrome_tasks_delaying_input_processing.sql',
            'duration_causing_jank_ms', '8');

        SELECT
          full_name,
          duration_ms,
          thread_dur_ms
        FROM chrome_tasks_delaying_input_processing;
        """,
        out=Path(
            'experimental_reliable_chrome_tasks_delaying_input_processing_test.out'
        ))

  def test_chrome_scroll_inputs_per_frame(self):
    return DiffTestBlueprint(
        trace=DataPath('scrolling_with_blocked_nonblocked_frames.pftrace'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_scroll_inputs_per_frame.sql');

        SELECT
          count_for_frame,
          ts
        FROM chrome_scroll_inputs_per_frame
        WHERE ts = 60934316798158;
        """,
        out=Csv("""
        "count_for_frame","ts"
        4,60934316798158
        """))

  def test_chrome_thread_slice_repeated(self):
    return DiffTestBlueprint(
        trace=Path('../track_event/track_event_counters.textproto'),
        query="""
        SELECT RUN_METRIC('chrome/chrome_thread_slice.sql');

        SELECT
          name,
          ts,
          dur,
          thread_dur
        FROM chrome_thread_slice;
        """,
        out=Csv("""
        "name","ts","dur","thread_dur"
        "event1_on_t1",1000,100,10000
        "event2_on_t1",2000,200,30000
        "event3_on_t1",2000,200,10000
        "event4_on_t1",4000,0,0
        "float_counter_on_t1",4300,0,"[NULL]"
        "float_counter_on_t1",4500,0,"[NULL]"
        "event1_on_t3",4000,100,5000
        """))

  def test_frame_times_metric(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_rendering_desktop.pftrace'),
        query=Metric('frame_times'),
        out=Path('frame_times_metric.out'))

  def test_chrome_dropped_frames_metric(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_rendering_desktop.pftrace'),
        query=Metric('chrome_dropped_frames'),
        out=TextProto(r"""
        [perfetto.protos.chrome_dropped_frames]: {
          dropped_frame: {
            ts: 166479338462000
            process_name: "Renderer"
            pid: 12743
          }
          dropped_frame: {
            ts: 166479355302000
            process_name: "Renderer"
            pid: 12743
          }
        }
        """))

  def test_chrome_long_latency_metric(self):
    return DiffTestBlueprint(
        trace=Path('../chrome/long_event_latency.textproto'),
        query="""
        SELECT RUN_METRIC('experimental/chrome_long_latency.sql');

        SELECT * FROM long_latency_with_process_info;
        """,
        out=Csv("""
        "ts","event_type","process_name","process_id"
        200111000,"FirstGestureScrollUpdate,GestureScrollUpdate","Renderer",1001
        200111000,"GestureScrollUpdate","Renderer",1002
        280111001,"GestureScrollUpdate","Renderer",1001
        """))

  def test_scroll_jank_mojo_simple_watcher(self):
    return DiffTestBlueprint(
        trace=Path('scroll_jank_mojo_simple_watcher.py'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank_cause_queuing_delay.sql');

        SELECT
          trace_id,
          jank,
          dur_overlapping_ns,
          metric_name
        FROM scroll_jank_cause_queuing_delay
        ORDER BY trace_id ASC, ts ASC;
        """,
        out=Path('scroll_jank_mojo_simple_watcher.out'))

  def test_scroll_jank_gpu_check(self):
    return DiffTestBlueprint(
        trace=Path('scroll_jank_gpu_check.py'),
        query="""
        SELECT RUN_METRIC('chrome/scroll_jank.sql');

        SELECT ts, jank
        FROM scroll_jank
        ORDER BY ts ASC;
        """,
        out=Csv("""
        "ts","jank"
        15000000,0
        30000000,1
        115000000,0
        """))
