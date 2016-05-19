# Copyright 2015 Google Inc. All Rights Reserved.
# coding=utf-8
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

"""Functions for generating bindings for overloaded operations/constructors."""

from bindings.scripts.v8_interface import distinguishing_argument_index
from bindings.scripts.v8_interface import effective_overload_set_by_length


def get_overload_contexts(method_overloads):
  """Create overload contexts for overloaded methods/constructors.

  Arguments:
      method_overloads: [overload_sets] where overload_sets is itself
          a list of method_contexts (or constructor_contexts). All methods
          in the list of method_contexts are overloads of the same symbol.
  Returns:
      [overload_contexts]
  """
  overload_contexts = []
  for overloaded_methods in method_overloads:
    # Create an overload context. Binding generation will use this as a top
    # level context, and the identifier will be bound to an overload
    # resolution function.
    overload_context = {
        'overloads': overloaded_methods,
    }

    # Add a property to the overloaded methods indicating their overload
    # indices. This allows us to refer to each overload with a unique name.
    for index, method in enumerate(overloaded_methods, 1):
      method['overload_index'] = index

    # Get the effective overload set, and group by the number of arguments.
    # This is a list of tuples (numargs, [overload_set])
    effective_overloads_by_length = (
        effective_overload_set_by_length(overloaded_methods))

    overload_context['length'] = min(
        [length for length, _ in effective_overloads_by_length])

    # The first four steps of the overload resolution algorithm involve
    # removing entries from the set S of all overloads where the number
    # of arguments in the overload does not match the number of overloads
    # passed to the function.
    # We accomplish this by partitioning the set S into S0...S{maxarg} where
    # each S{n} contains overloads with n arguments, and then switch on
    # the number of arguments in the bindings code.
    #
    # http://heycam.github.io/webidl/#dfn-overload-resolution-algorithm
    # 1. Let maxarg be the length of the longest type list of the entries
    #    in S.
    # 2. Initialize argcount to be min(maxarg, n).
    # 3. Remove from S all entries whose type list is not of length
    #    argcount.
    #
    # overload_resolution_context_by_length will be a list of tuples
    # (length, distinguishing_index, [overload_resolution_tests])
    overload_resolution_context_by_length = []
    for length, overload_set in effective_overloads_by_length:
      # For each overload, create a list of tuples for resolution:
      #   (length, distinguishing_argument_index, [resolution_tests])
      # where the length refers to the number of arguments for the set Sn of
      # overloads that take that number of arguments,
      # distinguishing_argument_index is as defined in the spec (except we
      # use None in the case that the size of the set Sn is 1), and
      # resolution tests is a list of tuples:
      #    (test_callable, method_context)
      # where test_callable is a function that takes one argument which is a
      # string representing the name of a variable in the generated bindings
      # code, and returns a boolean expression. If this evaluates to true,
      # then the test's associated overloaded method_context should be called.
      #
      # http://heycam.github.io/webidl/#dfn-overload-resolution-algorithm
      # 7. If there is more than one entry in S, then set d to be the
      #    distinguishing argument index for the entries of S.
      overload_resolution_context_by_length.append(
          (length,
           distinguishing_argument_index(overload_set)
           if len(overload_set) > 1 else None,
           list(resolution_tests_methods(overload_set))))
    overload_context['overload_resolution_by_length'] = (
        overload_resolution_context_by_length)

    overload_contexts.append(overload_context)

  return overload_contexts


def resolution_tests_methods(effective_overloads):
  """Yields resolution test and associated method.

  Tests and methods are in resolution order for effective overloads of a given
  length.

  This is the heart of the resolution algorithm.
  http://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

  This function is based off of v8_interface.resolution_tests_methods. In Blink
  the first item in the tuple is a string representing C++ code that can be
  executed to perform the test. In Cobalt it is a function that takes one
  argument which is a string representing the variable name of a C++ object
  on which the test should be performed, and returns an expression that
  performs the test.

  Args:
      effective_overloads: List of tuples returned from
          effective_overload_set_by_length
  Yields:
      (test, method)
  """

  methods = [effective_overload[0]
             for effective_overload in effective_overloads]
  if len(methods) == 1:
    # If only one method with a given length, no test needed
    yield lambda arg: 'true', methods[0]
    return

  # 6. If there is more than one entry in S, then set d to be the
  # distinguishing argument index for the entries of S.
  index = distinguishing_argument_index(effective_overloads)
  # (7-9 are for handling |undefined| values for optional arguments before
  # the distinguishing argument (as "missing"), so you can specify only some
  # optional arguments. We don't support this, so we skip these steps.)
  # 10. If i = d, then:
  # (d is the distinguishing argument index)
  # 1. Let V be argi.
  #     Note: This is the argument that will be used to resolve which
  #           overload is selected.
  # js_value = 'info[%s]' % index

  # Extract argument and IDL type to simplify accessing these in each loop.
  arguments = [method['arguments'][index] for method in methods]
  arguments_methods = zip(arguments, methods)
  idl_types = [argument['idl_type_object'] for argument in arguments]
  idl_types_methods = zip(idl_types, methods)

  # We can't do a single loop through all methods or simply sort them, because
  # a method may be listed in multiple steps of the resolution algorithm, and
  # which test to apply differs depending on the step.
  #
  # Instead, we need to go through all methods at each step, either finding
  # first match (if only one test is allowed) or filtering to matches (if
  # multiple tests are allowed), and generating an appropriate tests.

  # 2. If V is undefined, and there is an entry in S whose list of
  # optionality values has "optional" at index i, then remove from S all
  # other entries.
  try:
    method = next(method for argument, method in arguments_methods
                  if argument['is_optional'])
    yield (lambda arg: '%s.isUndefined()' % arg), method
  except StopIteration:
    pass

  # 3. Otherwise: if V is null or undefined, and there is an entry in S that
  # has one of the following types at position i of its type list,
  # - a nullable type
  try:
    method = next(method for idl_type, method in idl_types_methods
                  if idl_type.is_nullable)
    yield (lambda arg: '%s.isUndefinedOrNull()' % arg), method
  except StopIteration:
    pass

  # 4. Otherwise: if V is a platform object - but not a platform array
  # object - and there is an entry in S that has one of the following
  # types at position i of its type list,
  # - an interface type that V implements
  # (Unlike most of these tests, this can return multiple methods, since we
  #  test if it implements an interface. Thus we need a for loop, not a next.)
  # (We distinguish wrapper types from built-in interface types.)
  for idl_type, method in ((idl_type, method)
                           for idl_type, method in idl_types_methods
                           if idl_type.is_wrapper_type):
    base_type = idl_type.base_type if idl_type.is_nullable else idl_type
    # pylint raises a false-positive for referencing a variable in the loop.
    # The usage here is fine because
    def get_check_inherits_test(interface_type):
      format_string = '%%s.inherits(JSC%s::s_classinfo())' % interface_type
      return lambda arg: format_string % arg
    yield get_check_inherits_test(base_type), method

  # 8. Otherwise: if V is any kind of object except for a native Date object,
  # a native RegExp object, and there is an entry in S that has one of the
  # following types at position i of its type list,
  # - an array type
  # - a sequence type
  # ...
  # - a dictionary
  #
  # FIXME:
  # We don't strictly follow the algorithm here. The algorithm says "remove
  # all other entries" if there is "one entry" matching, but we yield all
  # entries to support following constructors:
  # [constructor(sequence<DOMString> arg), constructor(Dictionary arg)]
  # interface I { ... }
  # (Need to check array types before objects because an array is an object)
  for idl_type, method in idl_types_methods:
    if idl_type.native_array_element_type:
      raise NotImplementedError('Array types not yet supported')
  for idl_type, method in idl_types_methods:
    if idl_type.is_dictionary or idl_type.name == 'Dictionary':
      raise NotImplementedError('Dictionary types not yet supported.')

  # (Check for exact type matches before performing automatic type conversion;
  # only needed if distinguishing between primitive types.)
  if len([idl_type.is_primitive_type for idl_type in idl_types]) > 1:
    # (Only needed if match in step 11, otherwise redundant.)
    if any(idl_type.is_string_type or idl_type.is_enum
           for idl_type in idl_types):
      # 10. Otherwise: if V is a Number value, and there is an entry in S
      # that has one of the following types at position i of its type
      # list,
      # - a numeric type
      try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_numeric_type)
        yield (lambda arg: '%s.isNumber()' % arg), method
      except StopIteration:
        pass

  # (Perform automatic type conversion, in order. If any of these match,
  # that's the end, and no other tests are needed.) To keep this code simple,
  # we rely on the C++ compiler's dead code elimination to deal with the
  # redundancy if both cases below trigger.

  # 11. Otherwise: if there is an entry in S that has one of the following
  # types at position i of its type list,
  # - DOMString
  # - ByteString
  # - USVString
  # - an enumeration type
  try:
    method = next(method for idl_type, method in idl_types_methods
                  if idl_type.is_string_type or idl_type.is_enum)
    yield lambda arg: 'true', method
  except StopIteration:
    pass

  # 12. Otherwise: if there is an entry in S that has one of the following
  # types at position i of its type list,
  # - a numeric type
  try:
    method = next(method for idl_type, method in idl_types_methods
                  if idl_type.is_numeric_type)
    yield lambda arg: 'true', method
  except StopIteration:
    pass
