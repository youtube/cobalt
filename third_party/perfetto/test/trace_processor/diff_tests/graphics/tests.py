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


class Graphics(TestSuite):
  # Contains tests for graphics related events and tables. Graphics frame
  # trace tests.
  def test_graphics_frame_events(self):
    return DiffTestBlueprint(
        trace=Path('graphics_frame_events.py'),
        query="""
        SELECT ts, gpu_track.name AS track_name, dur, frame_slice.name AS slice_name,
          frame_number, layer_name
        FROM gpu_track
        LEFT JOIN frame_slice ON gpu_track.id = frame_slice.track_id
        WHERE scope = 'graphics_frame_event'
        ORDER BY ts;
        """,
        out=Path('graphics_frame_events.out'))

  # GPU Memory ftrace packets
  def test_gpu_mem_total(self):
    return DiffTestBlueprint(
        trace=Path('gpu_mem_total.py'),
        query=Path('gpu_mem_total_test.sql'),
        out=Csv("""
        "name","unit","description","ts","pid","value"
        "GPU Memory","7","Total GPU memory used by the entire system",0,"[NULL]",123
        "GPU Memory","7","Total GPU memory used by this process",0,1,100
        "GPU Memory","7","Total GPU memory used by the entire system",5,"[NULL]",256
        "GPU Memory","7","Total GPU memory used by this process",5,1,233
        "GPU Memory","7","Total GPU memory used by the entire system",10,"[NULL]",123
        "GPU Memory","7","Total GPU memory used by this process",10,1,0
        """))

  def test_gpu_mem_total_after_free_gpu_mem_total(self):
    return DiffTestBlueprint(
        trace=Path('gpu_mem_total_after_free.py'),
        query=Path('gpu_mem_total_test.sql'),
        out=Csv("""
        "name","unit","description","ts","pid","value"
        "GPU Memory","7","Total GPU memory used by this process",0,1,100
        "GPU Memory","7","Total GPU memory used by this process",5,1,233
        "GPU Memory","7","Total GPU memory used by this process",10,1,50
        """))

  # Clock sync
  def test_clock_sync(self):
    return DiffTestBlueprint(
        trace=Path('clock_sync.py'),
        query="""
        SELECT ts, cast(value AS integer) AS int_value
        FROM counters
        WHERE name GLOB 'gpu_counter*';
        """,
        out=Csv("""
        "ts","int_value"
        1,3
        102,5
        1003,7
        1005,9
        2006,11
        2010,12
        2013,13
        3007,14
        3010,15
        """))

  # Android SurfaceFlinger metrics
  def test_frame_missed_event_frame_missed(self):
    return DiffTestBlueprint(
        trace=Path('frame_missed.py'),
        query="""
        SELECT RUN_METRIC('android/android_surfaceflinger.sql');

        SELECT ts, dur
        FROM android_surfaceflinger_event;
        """,
        out=Csv("""
        "ts","dur"
        100,1
        102,1
        103,1
        """))

  def test_frame_missed_metrics(self):
    return DiffTestBlueprint(
        trace=Path('frame_missed.py'),
        query=Metric('android_surfaceflinger'),
        out=TextProto(r"""
        android_surfaceflinger {
          missed_frames: 3
          missed_hwc_frames: 0
          missed_gpu_frames: 0
          missed_frame_rate: 0.42857142857142855 # = 3/7
          gpu_invocations: 0
          metrics_per_display: {
            display_id: "101"
            missed_frames: 2
            missed_hwc_frames: 0
            missed_gpu_frames: 0
            missed_frame_rate: 0.5
          }
          metrics_per_display: {
            display_id: "102"
            missed_frames: 1
            missed_hwc_frames: 0
            missed_gpu_frames: 0
            missed_frame_rate: 0.33333333333333333
          }
        }
        """))

  def test_surfaceflinger_gpu_invocation(self):
    return DiffTestBlueprint(
        trace=Path('surfaceflinger_gpu_invocation.py'),
        query=Metric('android_surfaceflinger'),
        out=TextProto(r"""
        android_surfaceflinger {
          missed_frames: 0
          missed_hwc_frames: 0
          missed_gpu_frames: 0
          gpu_invocations: 4
          avg_gpu_waiting_dur_ms: 4
          total_non_empty_gpu_waiting_dur_ms: 11
        }
        """))

  # GPU metrics
  def test_gpu_metric(self):
    return DiffTestBlueprint(
        trace=Path('gpu_metric.py'),
        query=Metric('android_gpu'),
        out=TextProto(r"""
        android_gpu {
          processes {
            name: "app_1"
            mem_max: 8
            mem_min: 2
            mem_avg: 3
          }
          processes {
            name: "app_2"
            mem_max: 10
            mem_min: 6
            mem_avg: 8
          }
          mem_max: 4
          mem_min: 1
          mem_avg: 2
        }
        """))

  def test_gpu_frequency_metric(self):
    return DiffTestBlueprint(
        trace=Path('gpu_frequency_metric.textproto'),
        query=Metric('android_gpu'),
        out=Path('gpu_frequency_metric.out'))

  # Android Jank CUJ metric
  def test_android_jank_cuj(self):
    return DiffTestBlueprint(
        trace=Path('android_jank_cuj.py'),
        query=Metric('android_jank_cuj'),
        out=Path('android_jank_cuj.out'))

  def test_android_jank_cuj_query(self):
    return DiffTestBlueprint(
        trace=Path('android_jank_cuj.py'),
        query=Path('android_jank_cuj_query_test.sql'),
        out=Path('android_jank_cuj_query.out'))

  # Frame Timeline event trace tests
  def test_expected_frame_timeline_events(self):
    return DiffTestBlueprint(
        trace=Path('frame_timeline_events.py'),
        query=Path('expected_frame_timeline_events_test.sql'),
        out=Csv("""
        "ts","dur","pid","display_frame_token","surface_frame_token","layer_name"
        20,6,666,2,0,"[NULL]"
        21,15,1000,4,1,"Layer1"
        40,6,666,4,0,"[NULL]"
        41,15,1000,6,5,"Layer1"
        80,6,666,6,0,"[NULL]"
        90,16,1000,8,7,"Layer1"
        120,6,666,8,0,"[NULL]"
        140,6,666,12,0,"[NULL]"
        150,20,1000,15,14,"Layer1"
        170,6,666,15,0,"[NULL]"
        200,6,666,17,0,"[NULL]"
        220,10,666,18,0,"[NULL]"
        """))

  def test_actual_frame_timeline_events(self):
    return DiffTestBlueprint(
        trace=Path('frame_timeline_events.py'),
        query=Path('actual_frame_timeline_events_test.sql'),
        out=Path('actual_frame_timeline_events.out'))

  # Composition layer
  def test_composition_layer_count(self):
    return DiffTestBlueprint(
        trace=Path('composition_layer.py'),
        query="""
        SELECT RUN_METRIC('android/android_hwcomposer.sql');

        SELECT display_id, AVG(value)
        FROM total_layers
        GROUP BY display_id;
        """,
        out=Csv("""
        "display_id","AVG(value)"
        "0",3.000000
        "1",5.000000
        """))

  # G2D metrics TODO(rsavitski): find a real trace and double-check that the
  # is realistic. One kernel's source I checked had tgid=0 for all counter
  # Initial support was added/discussed in b/171296908.
  def test_g2d_metrics(self):
    return DiffTestBlueprint(
        trace=Path('g2d_metrics.textproto'),
        query=Metric('g2d'),
        out=Path('g2d_metrics.out'))

  # Composer execution
  def test_composer_execution(self):
    return DiffTestBlueprint(
        trace=Path('composer_execution.py'),
        query="""
        SELECT RUN_METRIC('android/composer_execution.sql',
          'output', 'hwc_execution_spans');

        SELECT
          validation_type,
          display_id,
          COUNT(*) AS count,
          SUM(execution_time_ns) AS total
        FROM hwc_execution_spans
        GROUP BY validation_type, display_id
        ORDER BY validation_type, display_id;
        """,
        out=Csv("""
        "validation_type","display_id","count","total"
        "separated_validation","1",1,200
        "skipped_validation","0",2,200
        "skipped_validation","1",1,100
        "unknown","1",1,0
        "unskipped_validation","0",1,200
        """))

  # Display metrics
  def test_display_metrics(self):
    return DiffTestBlueprint(
        trace=Path('display_metrics.py'),
        query=Metric('display_metrics'),
        out=TextProto(r"""
        display_metrics {
          total_duplicate_frames: 0
          duplicate_frames_logged: 0
          total_dpu_underrun_count: 0
          refresh_rate_switches: 5
          refresh_rate_stats {
            refresh_rate_fps: 60
            count: 2
            total_dur_ms: 2
            avg_dur_ms: 1
          }
          refresh_rate_stats {
            refresh_rate_fps: 90
            count: 2
            total_dur_ms: 2
            avg_dur_ms: 1
          }
          refresh_rate_stats {
            refresh_rate_fps: 120
            count: 1
            total_dur_ms: 2
            avg_dur_ms: 2
          }
          update_power_state {
            avg_runtime_micro_secs: 4000
          }
        }
        """))

  # DPU vote clock and bandwidth
  def test_dpu_vote_clock_bw(self):
    return DiffTestBlueprint(
        trace=Path('dpu_vote_clock_bw.textproto'),
        query=Metric('android_hwcomposer'),
        out=TextProto(r"""
        android_hwcomposer {
          skipped_validation_count: 0
          unskipped_validation_count: 0
          separated_validation_count: 0
          unknown_validation_count: 0
          dpu_vote_metrics {
            tid: 237
            avg_dpu_vote_clock: 206250
            avg_dpu_vote_avg_bw: 210000
            avg_dpu_vote_peak_bw: 205000
            avg_dpu_vote_rt_bw: 271000
          }
          dpu_vote_metrics {
            tid: 299
            avg_dpu_vote_clock: 250000
          }
        }
        """))

  # Video 4 Linux 2 related tests
  def test_v4l2_vidioc_slice(self):
    return DiffTestBlueprint(
        trace=Path('v4l2_vidioc.textproto'),
        query="""
        SELECT ts, dur, name
        FROM slice
        WHERE category = 'Video 4 Linux 2';
        """,
        out=Csv("""
        "ts","dur","name"
        593268475912,0,"VIDIOC_QBUF minor=0 seq=0 type=9 index=19"
        593268603800,0,"VIDIOC_QBUF minor=0 seq=0 type=9 index=20"
        593528238295,0,"VIDIOC_DQBUF minor=0 seq=0 type=9 index=19"
        593544028229,0,"VIDIOC_DQBUF minor=0 seq=0 type=9 index=20"
        """))

  def test_v4l2_vidioc_flow(self):
    return DiffTestBlueprint(
        trace=Path('v4l2_vidioc.textproto'),
        query="""
        SELECT qbuf.ts, qbuf.dur, qbuf.name, dqbuf.ts, dqbuf.dur, dqbuf.name
        FROM flow
        JOIN slice qbuf ON flow.slice_out = qbuf.id
        JOIN slice dqbuf ON flow.slice_in = dqbuf.id;
        """,
        out=Path('v4l2_vidioc_flow.out'))

  def test_virtio_video_slice(self):
    return DiffTestBlueprint(
        trace=Path('virtio_video.textproto'),
        query="""
        SELECT slice.ts, slice.dur, slice.name, track.name
        FROM slice
        JOIN track ON slice.track_id = track.id;
        """,
        out=Csv("""
        "ts","dur","name","name"
        593125003271,84500592,"Resource #102","virtio_video stream #4 OUTPUT"
        593125003785,100000,"RESOURCE_QUEUE","virtio_video stream #4 Requests"
        593125084611,709696,"Resource #62","virtio_video stream #3 OUTPUT"
        593125084935,100000,"RESOURCE_QUEUE","virtio_video stream #3 Requests"
        593125794194,100000,"RESOURCE_QUEUE","virtio_video stream #3 Responses"
        593209502603,100000,"RESOURCE_QUEUE","virtio_video stream #4 Responses"
        """))

  # virtgpu (drm/virtio) related tests
  def test_virtio_gpu(self):
    return DiffTestBlueprint(
        trace=Path('virtio_gpu.textproto'),
        query="""
        SELECT
          ts,
          dur,
          name
        FROM
          slice
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","name"
        1345090723759,1180312,"SUBMIT_3D"
        1345090746311,1167135,"CTX_DETACH_RESOURCE"
        """))

  # mali GPU events
  def test_mali(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          ftrace_events {
            cpu: 2
            event {
              timestamp: 751796307210
              pid: 2857
              mali_mali_KCPU_CQS_WAIT_START {
                info_val1: 1
                info_val2: 0
                kctx_tgid: 2201
                kctx_id: 10
                id: 0
              }
            }
            event {
              timestamp: 751800621175
              pid: 2857
              mali_mali_KCPU_CQS_WAIT_END {
                info_val1: 412313493488
                info_val2: 0
                kctx_tgid: 2201
                kctx_id: 10
                id: 0
              }
            }
            event {
              timestamp: 751800638997
              pid: 2857
              mali_mali_KCPU_CQS_SET {
                info_val1: 412313493480
                info_val2: 0
                kctx_tgid: 2201
                kctx_id: 10
                id: 0
              }
            }
          }
        }
        """),
        query="""
        SELECT ts, dur, name FROM slice WHERE name GLOB "mali_KCPU_CQS*";
        """,
        out=Csv("""
        "ts","dur","name"
        751796307210,4313965,"mali_KCPU_CQS_WAIT"
        751800638997,0,"mali_KCPU_CQS_SET"
        """))

  def test_mali_fence(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          ftrace_events {
            cpu: 2
            event {
              timestamp: 751796307210
              pid: 2857
              mali_mali_KCPU_FENCE_WAIT_START {
                info_val1: 1
                info_val2: 0
                kctx_tgid: 2201
                kctx_id: 10
                id: 0
              }
            }
            event {
              timestamp: 751800621175
              pid: 2857
              mali_mali_KCPU_FENCE_WAIT_END {
                info_val1: 412313493488
                info_val2: 0
                kctx_tgid: 2201
                kctx_id: 10
                id: 0
              }
            }
            event {
              timestamp: 751800638997
              pid: 2857
              mali_mali_KCPU_FENCE_SIGNAL {
                info_val1: 412313493480
                info_val2: 0
                kctx_tgid: 2201
                kctx_id: 10
                id: 0
              }
            }
          }
        }
        """),
        query="""
        SELECT ts, dur, name FROM slice WHERE name GLOB "mali_KCPU_FENCE*";
        """,
        out=Csv("""
        "ts","dur","name"
        751796307210,4313965,"mali_KCPU_FENCE_WAIT"
        751800638997,0,"mali_KCPU_FENCE_SIGNAL"
        """))
