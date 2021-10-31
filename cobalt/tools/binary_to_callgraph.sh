#!/bin/bash
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Create a callgraph from an x86 or x86-64 binary.
#
# Example Usage:
#     cobalt/tools/binary_to_callgraph.sh out/linux-x64x11_debug/cobalt > callgraph.txt
#
# TODO: It'd be cool to include "...->PostTask(...)" stuff in here too.
# TODO: It'd be cool to output to a sqlite database instead of text, and then
# have a command line program to search through it.
# TODO: It'd be cool to support ARM and MIPS too.

set -e

objdump -d "$1" |
python3 -c '
import collections
import re
import sys

caller = None
callee = None
graph = collections.defaultdict(set)
for line in sys.stdin:
  m = re.match(r"\w+ <(.*?)>:", line)
  if m:
    caller = m.group(1)
    continue

  m = re.match(".*callq?.*?<(.*?)>", line)
  if m:
    callee = m.group(1)
    graph[caller].add(callee)

for key, value in graph.items():
  for item in value:
    print(key)
    print(item)
' | c++filt |
python3 -c '
import sys

buffer = []
for line in sys.stdin:
  buffer.append(line.rstrip())
  if len(buffer) == 2:
    print("{} @@ {}".format(*buffer))
    buffer = []
'


