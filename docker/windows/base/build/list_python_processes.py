#!/usr/bin/env python

"""
Convenience script to debug the list of running python processes on windows
without relying on 3rd party GUI programs such as Process Explorer.
"""

import subprocess

wmic_cmd = """wmic process where "name='python.exe' or name='pythonw.exe'" get commandline,processid"""
wmic_prc = subprocess.Popen(wmic_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
wmic_out, _ = wmic_prc.communicate()

py_processes = [item.rsplit(None, 1) for item in wmic_out.splitlines() if item][1:]
py_processes = [[cmdline, int(pid)] for [cmdline, pid] in py_processes]

print('Running python processes:')
for p in py_processes:
  print(p[1]) # Prints the full python invocation.
  print(p[0]) # Prints the pid of the process.
  print
