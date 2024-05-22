#!/usr/bin/env python3
# Copyright (C) 2022 The Android Open Source Project
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

from os import sys, path

import synth_common

trace = synth_common.create_trace()

trace.add_packet()
trace.add_process(pid=1, ppid=0, cmdline="p1")
trace.add_process(pid=2, ppid=1, cmdline="p2")

trace.add_ftrace_packet(cpu=0)
trace.add_sched(ts=50, prev_pid=1, next_pid=2, prev_comm='t1', next_comm='t2')
trace.add_print(ts=51, tid=2, buf='N|2|track_p2|ev1\n')
trace.add_sched(ts=52, prev_pid=2, next_pid=1, prev_comm='t2', next_comm='t1')
trace.add_print(ts=53, tid=1, buf='N|1|track_p1|ev2\n')

sys.stdout.buffer.write(trace.trace.SerializeToString())
