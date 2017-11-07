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
"""V8c-specific implementation of CodeGeneratorCobalt.

Defines CodeGeneratorV8c and ExpressionGeneratorV8c classes.
"""

import os

from cobalt.bindings.code_generator_cobalt import CodeGeneratorCobalt
from cobalt.bindings.expression_generator import ExpressionGenerator


class ExpressionGeneratorV8c(ExpressionGenerator):
  """Implementation of ExpressionGenerator for V8."""

  def is_undefined(self, arg):
    return '{}->IsUndefined()'.format(arg)

  def is_undefined_or_null(self, arg):
    return '{}->IsNullOrUndefined()'.format(arg)

  def inherits_interface(self, interface_name, arg):
    return ('{}->IsObject() ? '
            'wrapper_factory->DoesObjectImplementInterface(object, '
            'base::GetTypeId<{}>()) : false').format(arg, interface_name)

  def is_number(self, arg):
    return '{}->IsNumber()'.format(arg)


class CodeGeneratorV8c(CodeGeneratorCobalt):
  """Code generator class for V8 bindings."""

  _expression_generator = ExpressionGeneratorV8c()

  def __init__(self, *args, **kwargs):
    module_path, _ = os.path.split(os.path.realpath(__file__))
    templates_dir = os.path.normpath(os.path.join(module_path, 'templates'))
    super(CodeGeneratorV8c, self).__init__(templates_dir, *args, **kwargs)

  @property
  def generated_file_prefix(self):
    return 'v8c'

  @property
  def expression_generator(self):
    return CodeGeneratorV8c._expression_generator
