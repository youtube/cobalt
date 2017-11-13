# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Provides functionality common for all tools."""

import ctypes
import os
import re
import subprocess
import sys


def GetPath(name, **kwargs):
  """Computes a path to a tool.

  All tools understand the same 4 path-related arguments: 'path', 'dir',
  'prefix', and 'name'. When the 'path' argument is provided, it overrides the
  path to a tool. Otherwise, 'dir', 'prefix', and 'name' arguments, all of which
  are optional, are joined as {dir}/{prefix}{name} to compute the path.

  Args:
    name: A default name of a tool.
    **kwargs: A dictionary that optionally contains 'path', 'dir', 'prefix', and
        'name' arguments.

  Returns:
    The computed path.
  """
  if 'path' in kwargs:
    return kwargs['path']

  path = kwargs.get('prefix', '') + kwargs.get('name', name)
  if 'dir' in kwargs:
    path = os.path.join(kwargs['dir'], path)
  return path


def GetRuleName(rule_name_base, toolset):
  """Computes a Ninja name for target and host rules."""
  suffix = '' if toolset == 'target' else '_{0}'.format(toolset)
  return rule_name_base + suffix


def EstimateMaxConcurrentLinkers():
  """Estimates a number of dynamic linkers to run concurrently.

  The estimate takes into account available RAM and conservatively assumes that
  Chromium is being built.

  Returns:
    An estimated number of processes.
  """
  # TODO: Introduce _TryGetPhysicalMemoryInBytes().
  if sys.platform in ('win32', 'cygwin'):

    class MEMORYSTATUSEX(ctypes.Structure):
      _fields_ = [
          ('dwLength', ctypes.c_ulong),
          ('dwMemoryLoad', ctypes.c_ulong),
          ('ullTotalPhys', ctypes.c_ulonglong),
          ('ullAvailPhys', ctypes.c_ulonglong),
          ('ullTotalPageFile', ctypes.c_ulonglong),
          ('ullAvailPageFile', ctypes.c_ulonglong),
          ('ullTotalVirtual', ctypes.c_ulonglong),
          ('ullAvailVirtual', ctypes.c_ulonglong),
          ('sullAvailExtendedVirtual', ctypes.c_ulonglong),
      ]  # pylint: disable=invalid-name

    stat = MEMORYSTATUSEX()
    stat.dwLength = ctypes.sizeof(stat)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))

    # VS 2015 uses 20% more working set than VS 2013 and can consume all RAM
    # on a 64 GB machine.
    return max(1, stat.ullTotalPhys / (5 * (2**30)))  # total / 5GB
  elif sys.platform.startswith('linux'):
    if os.path.exists('/proc/meminfo'):
      with open('/proc/meminfo') as meminfo:
        memtotal_re = re.compile(r'^MemTotal:\s*(\d*)\s*kB')
        for line in meminfo:
          match = memtotal_re.match(line)
          if not match:
            continue
          # Allow 6Gb per link on Linux because Gold is quite memory hungry
          return max(1, int(match.group(1)) / (6 * (2**20)))
    return 1
  elif sys.platform == 'darwin':
    try:
      avail_bytes = int(subprocess.check_output(['sysctl', '-n', 'hw.memsize'],
                                                shell=True))
      # A static library debug build of Chromium's unit_tests takes ~2.7GB, so
      # 4GB per ld process allows for some more bloat.
      return max(1, avail_bytes / (4 * (2**30)))  # total / 4GB
    except subprocess.CalledProcessError:
      return 1
  else:
    return 1
