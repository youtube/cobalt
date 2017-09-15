#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
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

# action_create_hash_table.py is a harness script to connect actions 
# sections of gyp-based builds to various build actions in JavaScriptCore
#
# usage: action_create_hash_table.py COMMAND -- OUTPUTS -- INPUTS
#
# Multiple OUTPUTS and INPUTS may be listed. The sections are
# separated by a -- argument.
#
# for each OUTPUT[i] we generate it by calling:
# calling COMMAND[0] INPUTS[i] COMMAND[1:] > OUTPUTS[i]

import os
import posixpath
import shutil
import subprocess
import sys

# borrowed from action_makenames.py
def SplitArgsIntoSections(args):
    sections = []
    while len(args) > 0:
        if not '--' in args:
            # If there is no '--' left, everything remaining is an entire section.
            dashes = len(args)
        else:
            dashes = args.index('--')

        sections.append(args[:dashes])

        # Next time through the loop, look at everything after this '--'.
        if dashes + 1 == len(args):
            # If the '--' is at the end of the list, we won't come back through the
            # loop again. Add an empty section now corresponding to the nothingness
            # following the final '--'.
            args = []
            sections.append(args)
        else:
            args = args[dashes + 1:]

    return sections

def main(args):
    sections = SplitArgsIntoSections(args[1:])
    assert len(sections) == 3
    command = sections[0]
    outputs = sections[1]
    inputs = sections[2]
    assert len(inputs) == len(outputs)
#    print 'command: %s' % command
#    print 'inputs: %s' % inputs
#    print 'outputs: %s' % outputs
  
    for i in range(0, len(inputs)):
      # make sure output directory exists
      out_dir = posixpath.dirname(outputs[i])
      if not posixpath.exists(out_dir):
        posixpath.makedirs(out_dir)
      # redirect stdout to the output file
      out_file = open(outputs[i], 'w')
      
      # build up the command
      subproc_cmd = ['perl', command[0]]
      subproc_cmd.append(inputs[i])
      subproc_cmd += command[1:]

      # fork and exec
      returnCode = subprocess.call(subproc_cmd, stdout=out_file)
      # clean up output file
      out_file.close()

      # make sure it worked
      assert returnCode == 0
    
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
