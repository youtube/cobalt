# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Compile an .idl file to Cobalt v8c bindings (.h and .cpp files).

Calls into idl_compiler_cobalt.shared_main specifying the V8
CodeGenerator class.
"""

import logging
import sys

import _env  #pylint: disable=import-error,unused-import
from cobalt.bindings.idl_compiler_cobalt import generate_bindings
from cobalt.bindings.v8c.code_generator_v8c import CodeGeneratorV8c


def main():
  # TODO(b/225964218, b/229148609): The flakiness here should be resolved and
  # retries removed. Retry up to 5 times as this can be flaky.
  latest_error = None
  for i in range(5):
    try:
      if i > 0:
        logging.warning('Failed IDL v8 compilation. Retry Attempt #%d', i)
      sys.exit(generate_bindings(CodeGeneratorV8c))
    except Exception as e:  #pylint: disable=broad-except
      logging.warning('Failed IDL v8 compilation. Cause: %s', str(e))
      latest_error = e
  if latest_error:
    raise latest_error


if __name__ == '__main__':
  main()
