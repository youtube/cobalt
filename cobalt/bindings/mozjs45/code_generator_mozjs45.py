# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

from cobalt.bindings.code_generator_cobalt import CodeGeneratorCobalt
from cobalt.bindings.expression_generator import ExpressionGenerator


class ExpressionGeneratorMozjs(ExpressionGenerator):
  """Implementation of ExpressionGenerator for SpiderMonkey 45."""

  def is_undefined(self, arg):
    return '%s.isUndefined()' % arg

  def is_undefined_or_null(self, arg):
    return '%s.isNullOrUndefined()' % arg

  def inherits_interface(self, interface_name, arg):
    return ('%s.isObject() ?'
            ' wrapper_factory->DoesObjectImplementInterface(\n'
            '              object, base::GetTypeId<%s>()) :\n'
            '              false') % (arg, interface_name)

  def is_number(self, arg):
    return '%s.isNumber()' % arg

  def is_type(self, interface_name, arg):
    return ('{}.isObject() ? '
            'JS_Is{}Object(object): false').format(arg, interface_name)


class CodeGeneratorMozjs45(CodeGeneratorCobalt):
  """Code generator class for SpiderMonkey45 bindings."""

  _expression_generator = ExpressionGeneratorMozjs()

  def __init__(self, *args, **kwargs):
    module_path, _ = os.path.split(os.path.realpath(__file__))
    templates_dir = os.path.normpath(os.path.join(module_path, 'templates'))
    super(CodeGeneratorMozjs45, self).__init__(templates_dir, *args, **kwargs)

  @property
  def generated_file_prefix(self):
    return 'mozjs'

  @property
  def expression_generator(self):
    return CodeGeneratorMozjs45._expression_generator
