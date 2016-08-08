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
"""Mozjs-specific implementation of CodeGeneratorCobalt.

Defines CodeGeneratorMozjs and ExpressionGeneratorMozjs classes.
"""

import os
import sys

# Add the bindings directory to the path.
module_path, module_filename = os.path.split(os.path.realpath(__file__))
cobalt_bindings_dir = os.path.normpath(os.path.join(module_path, os.pardir))
sys.path.append(cobalt_bindings_dir)

from code_generator_cobalt import CodeGeneratorCobalt  # pylint: disable=g-import-not-at-top
from expression_generator import ExpressionGenerator

TEMPLATES_DIR = os.path.normpath(os.path.join(module_path, 'templates'))


class ExpressionGeneratorMozjs(ExpressionGenerator):
  """Implementation  of ExpressionGenerator for JavaScriptCore."""

  def is_undefined(self, arg):
    return '%s.isUndefined()' % arg

  def is_undefined_or_null(self, arg):
    return '%s.isNullOrUndefined()' % arg

  def inherits_interface(self, interface_name, arg):
    # TODO: Implement this.
    return 'false'

  def is_number(self, arg):
    return '%s.isNumber()' % arg


class CodeGeneratorMozjs(CodeGeneratorCobalt):
  """Code generator class for JavaScriptCore binidings."""

  _expression_generator = ExpressionGeneratorMozjs()

  def __init__(self, interfaces_info, cache_dir, output_dir):
    CodeGeneratorCobalt.__init__(self, interfaces_info, TEMPLATES_DIR,
                                 cache_dir, output_dir)

  @property
  def generated_file_prefix(self):
    return 'Mozjs'

  @property
  def expression_generator(self):
    return CodeGeneratorMozjs._expression_generator
