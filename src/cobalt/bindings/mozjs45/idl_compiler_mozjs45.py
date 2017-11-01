# Copyright 2016 Google Inc. All Rights Reserved.
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
"""Compile an .idl file to Cobalt mozjs bindings (.h and .cpp files).

Calls into idl_compiler_cobalt.shared_main specifying the SpiderMonkey
CodeGenerator class.
"""

import sys

import _env  # pylint: disable=unused-import
from cobalt.bindings.idl_compiler_cobalt import generate_bindings
from cobalt.bindings.mozjs45.code_generator_mozjs45 import CodeGeneratorMozjs45

if __name__ == '__main__':
  sys.exit(generate_bindings(CodeGeneratorMozjs45))
