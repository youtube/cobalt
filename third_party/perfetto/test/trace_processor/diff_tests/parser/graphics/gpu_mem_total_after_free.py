#!/usr/bin/env python3
# Copyright (C) 2020 The Android Open Source Project
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

# gpu_mem_total initial counter event for pid = 1
trace.add_gpu_mem_total_event(pid=1, ts=0, size=100)

# gpu_mem_total ftrace event for pid = 1
trace.add_ftrace_packet(cpu=1)
trace.add_gpu_mem_total_ftrace_event(ftrace_pid=1, pid=1, ts=5, size=233)
trace.add_gpu_mem_total_ftrace_event(ftrace_pid=1, pid=1, ts=10, size=50)

# Finish the process at ts = 12
trace.add_process_free(ts=12, tid=1, comm="app", prio=10)

# Emit another event now at ts = 15 - this should be ignored
# (see b/192274404 for more info).
trace.add_gpu_mem_total_ftrace_event(ftrace_pid=0, pid=1, ts=15, size=0)

sys.stdout.buffer.write(trace.trace.SerializeToString())
