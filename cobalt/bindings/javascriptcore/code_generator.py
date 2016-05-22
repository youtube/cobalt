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
"""JSC-specific implementation of CodeGeneratorCobalt.

Defines CodeGeneratorJsc and ExpressionGeneratorJsc classes.
"""

import os
import sys

# Add the bindings directory to the path.
module_path, module_filename = os.path.split(os.path.realpath(__file__))
cobalt_bindings_dir = os.path.normpath(os.path.join(module_path, os.pardir))
sys.path.append(cobalt_bindings_dir)

from bindings.scripts.idl_types import IdlType  # pylint: disable=g-import-not-at-top
from code_generator_cobalt import CodeGeneratorCobalt
from expression_generator import ExpressionGenerator

TEMPLATES_DIR = os.path.normpath(os.path.join(module_path, 'templates'))


def calculate_jsc_lookup_size(num_properties):
  """Calculate the number of entries to allocate in JSC::Lookup hash table."""
  # JSC::Lookup uses an array internally to represent hash entries.
  # The first |size_mask + 1| entries are hash buckets, and the remaining
  # entries starting from entries[size_mask+1] are used for hash collisions.
  # In the worst case where all keys collide, we will need
  # (size_mask +1) + (num_properties - 1) entries, to account for properties[0]
  # getting assigned to a hash bucket, and the remaining N-1 properties getting
  # assigned to overflow entries.
  # TODO(***REMOVED***): WebKit's bindings generation script calculates the actual
  # required number of entries by duplicating the JSC::Lookup logic in the
  # script and determining the number of collisions. If we want to reduce the
  # number of entries allocated we can do something similar.
  size_mask = calculate_jsc_lookup_size_mask(num_properties)
  return (size_mask + 1) + max(0, (num_properties - 1))


def calculate_jsc_lookup_size_mask(num_properties):
  """Calculate the number of hash buckets to use in JSC::Lookup hash table."""
  # 2 * num_properties is the heuristic used in WebKit's JSC bindings generation
  # script to determine the number of hash buckets for the property hash table
  num_hash_buckets = max(1, 2 * num_properties)
  # JSC::Lookup hash table implementation uses a bitmask of the Identifier's
  # hash to determine which bucket a Property goes into. This also rounds the
  # number of buckets up to the closest power of 2.
  return pow(2, num_hash_buckets.bit_length()) - 1


class ExpressionGeneratorJsc(ExpressionGenerator):
  """Implementation  of ExpressionGenerator for JavaScriptCore."""

  def is_undefined(self, arg):
    return '%s.isUndefined()' % arg

  def is_undefined_or_null(self, arg):
    return '%s.isUndefinedOrNull()' % arg

  def inherits_interface(self, interface_name, arg):
    return '%s.inherits(JSC%s::s_classinfo())' % (arg, interface_name)

  def is_number(self, arg):
    return '%s.isNumber()' % arg


class CodeGeneratorJsc(CodeGeneratorCobalt):
  """Code generator class for JavaScriptCore binidings."""

  _expression_generator = ExpressionGeneratorJsc()

  def __init__(self, interfaces_info, cache_dir, output_dir):
    CodeGeneratorCobalt.__init__(self, interfaces_info, TEMPLATES_DIR,
                                 cache_dir, output_dir)

  @property
  def generated_file_prefix(self):
    return 'JSC'

  @property
  def expression_generator(self):
    return CodeGeneratorJsc._expression_generator

  def build_interface_context(self, interface, interface_info, definitions):
    """Overrides CodeGeneratorCobalt.build_interface_context."""
    context = super(CodeGeneratorJsc, self).build_interface_context(
        interface, interface_info, definitions)
    jsc_lookup_info = {
        'calculate_jsc_lookup_size': calculate_jsc_lookup_size,
        'calculate_jsc_lookup_size_mask': calculate_jsc_lookup_size_mask,
    }
    context.update(jsc_lookup_info)
    return context

  def referenced_interface_contexts(self, interface_name):
    """Overrides CodeGeneratorCobalt.referenced_interface_contexts."""
    for context in super(CodeGeneratorJsc,
                         self).referenced_interface_contexts(interface_name):
      yield context

    # Callback interface binding logic is currently JSC-specific.
    if interface_name in IdlType.callback_interfaces:
      # EventListener interface needs special case handling.
      assert interface_name == 'EventListener'
      yield {
          'fully_qualified_name':
              'cobalt::script::javascriptcore::JSCEventListenerHolder',
          'include': 'cobalt/script/javascriptcore/jsc_event_listener_holder.h',
          'conditional': None,
          'is_callback_interface': True,
      }
