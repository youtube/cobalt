#!/usr/bin/env python3
# Copyright (C) 2018 The Android Open Source Project
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

from os import sys

import synth_common

trace = synth_common.create_trace()
trace.add_packet()
trace.add_process(1, 0, 'init')
trace.add_process(2, 1, 'system_server')
trace.add_process(3, 1, 'com.google.android.calendar', 10003)
trace.add_process(4, 1, 'com.google.android.calculator', 10004)
trace.add_process(5, 1, 'com.google.android.deskclock', 10005)
trace.add_process(6, 1, 'com.google.android.gm', 10006)
trace.add_process(10, 1, 'dex2oat64')
trace.add_process(11, 1, 'installd')

trace.add_package_list(
    ts=1, name='com.google.android.calendar', uid=10003, version_code=123)
trace.add_package_list(
    ts=2, name='com.google.android.calculator', uid=10004, version_code=123)
trace.add_package_list(
    ts=3, name='com.google.android.deskclock', uid=10005, version_code=123)
trace.add_package_list(
    ts=4, name='com.google.android.gm', uid=10006, version_code=123)

trace.add_ftrace_packet(cpu=0)

# First launch: don't have either dex2oat or installd
trace.add_atrace_async_begin(ts=100, tid=2, pid=2, buf='launchingActivity#1')
trace.add_atrace_async_end(ts=200, tid=2, pid=2, buf='launchingActivity#1')
trace.add_atrace_instant(
    ts=201,
    tid=2,
    pid=2,
    buf='launchingActivity#1:completed:com.google.android.calendar')

# Second launch: just dex2oat
trace.add_atrace_async_begin(ts=300, tid=2, pid=2, buf='launchingActivity#2')
trace.add_sched(ts=305, prev_pid=0, next_pid=10)
trace.add_sched(ts=310, prev_pid=10, next_pid=0)
trace.add_atrace_async_end(ts=400, tid=2, pid=2, buf='launchingActivity#2')
trace.add_atrace_instant(
    ts=401,
    tid=2,
    pid=2,
    buf='launchingActivity#2:completed:com.google.android.calculator')

# Third launch: just installd
trace.add_atrace_async_begin(ts=500, tid=2, pid=2, buf='launchingActivity#3')
trace.add_sched(ts=505, prev_pid=0, next_pid=11)
trace.add_sched(ts=510, prev_pid=11, next_pid=0)
trace.add_atrace_async_end(ts=600, tid=2, pid=2, buf='launchingActivity#3')
trace.add_atrace_instant(
    ts=601,
    tid=2,
    pid=2,
    buf='launchingActivity#3:completed:com.google.android.deskclock')

# Third launch: just installd
trace.add_atrace_async_begin(ts=700, tid=2, pid=2, buf='launchingActivity#4')
trace.add_sched(ts=705, prev_pid=0, next_pid=10)
trace.add_sched(ts=710, prev_pid=10, next_pid=0)
trace.add_sched(ts=715, prev_pid=0, next_pid=11)
trace.add_sched(ts=720, prev_pid=11, next_pid=0)
trace.add_atrace_async_end(ts=800, tid=2, pid=2, buf='launchingActivity#4')
trace.add_atrace_instant(
    ts=801,
    tid=2,
    pid=2,
    buf='launchingActivity#4:completed:com.google.android.gm')

sys.stdout.buffer.write(trace.trace.SerializeToString())
