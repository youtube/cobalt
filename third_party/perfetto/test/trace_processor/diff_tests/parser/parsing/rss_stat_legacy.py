#!/usr/bin/env python3
# Copyright (C) 2019 The Android Open Source Project
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

# This synthetic trace tests legacy rss_stat events on old kernels which
# do not have the mm_id change and thus we cannot accurately track
# rss changes when one process changes another's mm struct.

from os import sys, path

import synth_common

trace = synth_common.create_trace()

trace.add_packet(ts=1)
trace.add_process(10, 0, "process")

trace.add_ftrace_packet(0)

# Create a kernel "thread", which is a single-thread process child of kthreadd.
trace.add_newtask(ts=50, tid=2, new_tid=3, new_comm="kthreadd_child", flags=0)

# Add an event on tid 3 which affects its own rss.
trace.add_rss_stat(ts=90, tid=3, member=0, size=9)

# Add an event on tid 10 from tid 3. This emulates e.g. background reclaim
# where kthreadd is cleaning up the mm struct of another process.
trace.add_rss_stat(ts=91, tid=3, member=0, size=900)

# Add an event for tid 3 from tid 10. This emulates e.g. direct reclaim
# where a process reaches into another process' mm struct.
trace.add_rss_stat(ts=99, tid=10, member=0, size=10)

# Add an event on tid 10 which affects its own rss.
trace.add_rss_stat(ts=100, tid=10, member=0, size=1000)

# Add an event on tid 10 from tid 3.
trace.add_rss_stat(ts=101, tid=3, member=0, size=900)

sys.stdout.buffer.write(trace.trace.SerializeToString())
