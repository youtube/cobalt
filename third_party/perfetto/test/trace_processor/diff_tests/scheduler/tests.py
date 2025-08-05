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


class Scheduler(TestSuite):
  # Scheduler
  def test_sched_cpu_util_cfs(self):
    return DiffTestBlueprint(
        trace=Path('sched_cpu_util_cfs.textproto'),
        query=Path('sched_cpu_util_cfs_test.sql'),
        out=Csv("""
        "name","ts","value"
        "Cpu 6 Util",10000,1.000000
        "Cpu 6 Cap",10000,1004.000000
        "Cpu 6 Nr Running",10000,0.000000
        "Cpu 7 Util",11000,1.000000
        "Cpu 7 Cap",11000,1007.000000
        "Cpu 7 Nr Running",11000,0.000000
        "Cpu 4 Util",12000,43.000000
        "Cpu 4 Cap",12000,760.000000
        "Cpu 4 Nr Running",12000,0.000000
        "Cpu 5 Util",13000,125.000000
        "Cpu 5 Cap",13000,757.000000
        "Cpu 5 Nr Running",13000,1.000000
        """))
