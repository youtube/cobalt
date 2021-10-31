# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
"""Utilities for converting between IDL and Cobalt naming conventions."""

import re

# Matches the boundary between words in CamelCase
# For example, if a space was inserted at the matched boundaries:
#     JSCTestEmptyHTML5Interface -> JSC Test Empty HTML5 Interface
# (?<!^) ensures that we don't match the first character in the string
#     as a word boundary
# (?<![A-Z])(?=[A-Z]) will match an uppercase character that is
#     preceded by a non-uppercase character: FooBar -> Foo Bar
# (?=[A-Z][a-z]) will match an uppercase character that is followed by
#     a lowercase character: FOOBar -> FOO Bar
titlecase_word_delimiter_re = re.compile(
    '(?<!^)((?<![A-Z])(?=[A-Z])|(?=[A-Z][a-z]))')

# Terms that should always be marked as words regardless of the case of
# neighboring characters.
word_list = ['html']

# List of special tokens which are always be marked as words.
special_token_list = ['3d']

# Regular expression to capture all of the special tokens.
special_token_re = re.compile(
    '(%s)' % '|'.join(special_token_list), flags=re.IGNORECASE)

# Split tokens on non-alphanumeric characters (excluding underscores).
enumeration_value_word_delimeter_re = re.compile(r'[^a-zA-Z0-9]')


def convert_to_regular_cobalt_name(class_name):
  cobalt_name = titlecase_word_delimiter_re.sub('_', class_name).lower()
  for term in word_list:
    replacement = [
        token for token in re.split('_?(%s)_?' %
                                    term, cobalt_name, re.IGNORECASE) if token
    ]
    cobalt_name = '_'.join(replacement)
  return cobalt_name


def convert_to_cobalt_name(class_name):
  """Convert an IDL attribute name to Cobalt naming convention."""
  replacement = []

  for token in special_token_re.split(class_name):
    if not token:
      continue

    if token.lower() in special_token_list:
      replacement.append(token.lower())
    else:
      replacement.append(convert_to_regular_cobalt_name(token))

  return '_'.join(replacement)


def capitalize_function_name(operation_name):
  return operation_name[0].capitalize() + operation_name[1:]


def convert_to_cobalt_constant_name(constant_name):
  return 'k' + ''.join(
      (token.capitalize() for token in constant_name.split('_')))


def convert_to_cobalt_enumeration_value(enum_type, enum_value):
  return 'k%s%s' % (enum_type, ''.join(
      (token.capitalize()
       for token in enumeration_value_word_delimeter_re.split(enum_value))))


def get_interface_name(idl_type):
  """Get the underlying interface name from an idl_type."""
  assert (idl_type.is_interface_type or idl_type.is_callback_function or
          idl_type.is_enum or idl_type.is_dictionary)
  if idl_type.is_nullable:
    return idl_type.inner_type.name
  elif idl_type.name.endswith('ConstructorConstructor'):
    return idl_type.name[0:-len('ConstructorConstructor')]
  elif idl_type.name.endswith('Constructor'):
    return idl_type.constructor_type_name
  else:
    return idl_type.name
