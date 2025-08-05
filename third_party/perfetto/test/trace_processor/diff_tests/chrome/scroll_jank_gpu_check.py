#!/usr/bin/env python3
# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Discarded events that do not get to GPU are invisible for UMA metric and
# therefore should be excluded in trace-based metric. This tests ensures that's
# the case.

from os import sys

import synth_common

from synth_common import ms_to_ns
trace = synth_common.create_trace()


class Helper:

  def __init__(self, trace, start_id, start_gesture_id):
    self.trace = trace
    self.id = start_id
    self.gesture_id = start_gesture_id

  def begin(self, from_ms, dur_ms):
    self.trace.add_input_latency_event_slice(
        "GestureScrollBegin",
        ts=ms_to_ns(from_ms),
        dur=ms_to_ns(dur_ms),
        track=self.id,
        trace_id=self.id,
        gesture_scroll_id=self.gesture_id,
    )
    self.id += 1

  def update(self, from_ms, dur_ms, gets_to_gpu=True):
    self.trace.add_input_latency_event_slice(
        "GestureScrollUpdate",
        ts=ms_to_ns(from_ms),
        dur=ms_to_ns(dur_ms),
        track=self.id,
        trace_id=self.id,
        gesture_scroll_id=self.gesture_id,
        gets_to_gpu=gets_to_gpu,
        is_coalesced=False,
    )
    self.id += 1

  def end(self, from_ms, dur_ms):
    self.trace.add_input_latency_event_slice(
        "GestureScrollEnd",
        ts=ms_to_ns(from_ms),
        dur=ms_to_ns(dur_ms),
        track=self.id,
        trace_id=self.id,
        gesture_scroll_id=self.gesture_id)
    self.id += 1
    self.gesture_id += 1


helper = Helper(trace, start_id=1234, start_gesture_id=5678)

helper.begin(from_ms=0, dur_ms=10)
helper.update(from_ms=15, dur_ms=10)
# The next update should be recognized as janky
helper.update(from_ms=30, dur_ms=30)
helper.end(from_ms=70, dur_ms=10)

helper.begin(from_ms=100, dur_ms=10)
helper.update(from_ms=115, dur_ms=10)
# The next update doesn't get to GPU, therefore would not be a part of jank
# calculation
helper.update(from_ms=130, dur_ms=30, gets_to_gpu=False)
helper.end(from_ms=170, dur_ms=10)

sys.stdout.buffer.write(trace.trace.SerializeToString())
