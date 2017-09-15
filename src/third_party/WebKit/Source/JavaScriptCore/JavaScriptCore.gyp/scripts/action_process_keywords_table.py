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

# JSC's DerivedSources.make calls for two different actions to happen to the
# Keywords.table file.  The first generates Lexer.lut.h, and the second 
# generated Keywords.h.  gyp can handle the concept of two different actions
# with the same dependent input file but this is breaking the MSVS project
# generation.  I'm writing this python wrapper to act as a single build action
# within the gyp but that will do both things to that file.

import os
import posixpath
import shutil
import subprocess
import sys

def main(args):
  if len(args) != 3:
    print "usage: action_process_keywords_table.py /path/to/Keywords.table /path/to/output/dir"
    sys.exit(-1)
  kw_path = args[1]
  output_dir_path = args[2]
  # it is assumed that the scripts live one higher than the current working dir
  script_path = os.path.abspath(os.path.join(os.getcwd(), '..'))
    
  # first command:
  # ../create_hash_table /path/to/Keywords.table > /path/to/output/dir/Lexer.lut.h
  if not os.path.exists(output_dir_path):
    os.makedirs(output_dir_path)
  out_file = open(os.path.join(output_dir_path, 'Lexer.lut.h'), 'w')
  subproc_cmd = ['perl', os.path.join(script_path, 'create_hash_table'), kw_path]
  return_code = subprocess.call(subproc_cmd, stdout=out_file)
  out_file.close()
  assert(return_code == 0)

  # second command:
  # python ../KeywordLookupGenerator.py  > /path/to/output/dir/KeywordLookup.h
  out_file = open(os.path.join(output_dir_path, 'KeywordLookup.h'), 'w')
  subproc_cmd = ['python', '../KeywordLookupGenerator.py', kw_path]
  return_code = subprocess.call(subproc_cmd, stdout=out_file)
  out_file.close()
  assert(return_code == 0)
  
  return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
