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
"""Create contexts to be used by Jinja template generation.

Extract the relevant information from the IdlParser objects and store them in
dicts that will be used by Jinja in JS bindings generation.
"""

import _env  # pylint: disable=unused-import
from idl_definitions import IdlTypedef
from idl_types import IdlPromiseType
from idl_types import IdlSequenceType
from name_conversion import capitalize_function_name
from name_conversion import convert_to_cobalt_constant_name
from name_conversion import convert_to_cobalt_enumeration_value
from name_conversion import convert_to_cobalt_name
from name_conversion import get_interface_name
from overload_context import get_overload_contexts
from v8_attributes import is_constructor_attribute
from v8_interface import method_overloads_by_name


def is_date_type(idl_type):
  return str(idl_type) == 'Date'


def is_object_type(idl_type):
  return str(idl_type) == 'object'


def is_any_type(idl_type):
  return idl_type.name == 'Any'


def is_sequence_type(idl_type):
  return isinstance(idl_type, IdlSequenceType)


def is_promise_type(idl_type):
  return isinstance(idl_type, IdlPromiseType)


def idl_literal_to_cobalt_literal(idl_type, idl_literal):
  """Map IDL literal to the corresponding cobalt value."""
  if idl_literal.is_null and not idl_type.is_interface_type:
    return 'base::nullopt'
  if idl_type.is_enum:
    return convert_to_cobalt_enumeration_value(idl_type, idl_literal.value)
  return str(idl_literal)


def get_dictionary_default_value(idl_type, idl_literal, name):
  """Mapping to cobalt value filtering for dictionary acceptable values."""
  if is_any_type(idl_type) and not idl_literal.is_null:
    raise ValueError('Unsupported default value in dictionary: '
                     '\'%s %s = %s\'. Only null default is supported.' %
                     (idl_type, name, idl_literal))
  return idl_literal_to_cobalt_literal(idl_type, idl_literal)


def idl_primitive_type_to_cobalt(idl_type):
  """Map IDL primitive type to C++ type."""
  type_map = {
      'boolean': 'bool',
      'byte': 'int8_t',
      'octet': 'uint8_t',
      'short': 'int16_t',
      'unsigned short': 'uint16_t',
      'long': 'int32_t',
      'long long': 'int64_t',
      'unsigned long': 'uint32_t',
      'unsigned long long': 'uint64_t',
      'float': 'float',
      'unrestricted float': 'float',
      'double': 'double',
      'unrestricted double': 'double',
  }
  assert idl_type.is_primitive_type, 'Expected primitive type.'
  return type_map[idl_type.base_type]


def idl_string_type_to_cobalt(idl_type):
  """Map IDL string type to C++ type."""
  type_map = {
      'ByteString': 'std::vector<uint8_t>',
      'DOMString': 'std::string',
      'String': 'std::string',
      'StringOrNull': 'std::string',
      'USVString': 'std::string',
  }
  assert idl_type.is_string_type, 'Expected string type.'
  return type_map[idl_type.name]


def cobalt_type_is_optional(idl_type):
  """Return True iff the idl_type should be wrapped by a base::optional<>.

  Returns:
    (bool): Whether the cobalt type should be wrapped in base::optional<>.
  Args:
    idl_type: An idl_types.IdlType object.

  The Cobalt type for interfaces and callback functions are scoped_refptr, so
  they can already be assigned a NULL value. Other types, such as primitives,
  strings, and unions, need to be wrapped by base::optional<>, in which case
  the IDL null value will map to base::nullopt_t.
  """

  # These never need base::optional<>
  if (idl_type.is_interface_type or idl_type.is_callback_function or
      idl_type.is_callback_interface or is_object_type(idl_type) or
      is_any_type(idl_type)):
    return False

  # We consider a union type to be nullable if either the entire union is
  # nullable, or one of its member types are.
  return (idl_type.is_nullable or
          (idl_type.is_union_type and
           (idl_union_type_has_nullable_member(idl_type))))


def get_optional_arguments(arguments):
  """Create optional arguments list."""
  return [argument for argument in arguments if argument['is_optional']]


def get_non_optional_arguments(arguments):
  """Create non optional arguments list."""
  return [
      argument for argument in arguments
      if not argument['is_optional'] and not argument['is_variadic']
  ]


def get_num_default_arguments(optional_arguments):
  """Return the number of default arguments."""
  num_default_arguments = 0

  for argument in optional_arguments:
    if argument['default_value'] is not None:
      num_default_arguments += 1

  return num_default_arguments


def get_variadic_argument(arguments):
  """Return the variadic argument."""
  length = len(arguments)

  if length > 0 and arguments[length - 1]['is_variadic']:
    return arguments[length - 1]
  else:
    return []


def idl_union_type_has_nullable_member(idl_type):
  """Return True iff the idl_type is a union with a nullable member."""
  assert idl_type.is_union_type, 'Expected union type.'
  for member in idl_type.member_types:
    if member.is_nullable:
      return True
    elif member.is_union_type and idl_union_type_has_nullable_member(member):
      return True
  return False


def get_conversion_flags(idl_type, extended_attributes):
  """Build an expression setting a bitmask of flags used for conversion."""
  assert not isinstance(idl_type, IdlTypedef)
  flags = []
  # Flags must correspond to the enumeration in
  # scripts/javascriptcore/conversion_helpers.h
  if (idl_type.is_numeric_type and not idl_type.is_integer_type and
      not idl_type.base_type.startswith('unrestricted ')):
    flags.append('kConversionFlagRestricted')
  if idl_type.is_nullable and not cobalt_type_is_optional(idl_type.inner_type):
    # Other types use base::optional<> so there is no need for a flag to check
    # if null values are allowed.
    flags.append('kConversionFlagNullable')
  if idl_type.is_string_type:
    if extended_attributes.get('TreatNullAs', '') == 'EmptyString':
      flags.append('kConversionFlagTreatNullAsEmptyString')
    elif extended_attributes.get('TreatUndefinedAs', '') == 'EmptyString':
      flags.append('kConversionFlagTreatUndefinedAsEmptyString')

  if extended_attributes.has_key('Clamp'):
    flags.append('kConversionFlagClamped')

  if is_object_type(idl_type):
    flags.append('kConversionFlagObjectOnly')

  if flags:
    return '(%s)' % ' | '.join(flags)
  else:
    return 'kNoConversionFlags'


class ContextBuilder(object):
  """Build jinja2 contexts (python dicts) for use in bindings generation."""

  def __init__(self, info_provider):
    self.info_provider = info_provider

  def resolve_typedef(self, idl_type):
    idl_type = idl_type.resolve_typedefs(self.info_provider.typedefs)
    if isinstance(idl_type, IdlTypedef):
      idl_type = idl_type.idl_type
    return idl_type

  def idl_sequence_type_to_cobalt(self, idl_type):
    """Map IDL sequence type to C++ sequence type implementation."""
    assert is_sequence_type(idl_type), 'Expected sequence type.'
    element_idl_type = idl_type.element_type
    assert not is_object_type(element_idl_type), 'Object type not supported.'
    assert (not element_idl_type.is_callback_function and
            not idl_type.is_callback_interface), 'Callback types not supported.'
    element_cobalt_type = self.idl_type_to_cobalt_type(
        self.resolve_typedef(element_idl_type))
    return '::cobalt::script::Sequence< %s >' % element_cobalt_type

  def idl_union_type_to_cobalt(self, idl_type):
    """Map IDL union type to C++ union type implementation."""
    # Flatten the union type. Order matters for our implementation.
    assert idl_type.is_union_type, 'Expected union type.'
    flattened_types = []
    for member in idl_type.member_types:
      member = self.resolve_typedef(member)
      if member.is_nullable:
        member = member.inner_type

      if member.is_union_type:
        flattened_types.extend(self.idl_union_type_to_cobalt(member))
      else:
        flattened_types.append(member)

    cobalt_types = [self.idl_type_to_cobalt_type(t) for t in flattened_types]
    return '::cobalt::script::UnionType%d<%s >' % (len(cobalt_types),
                                                   ', '.join(cobalt_types))

  def idl_type_to_cobalt_type(self, idl_type):
    """Map IDL type to C++ type."""
    assert not isinstance(idl_type, IdlTypedef)
    cobalt_type = None
    if idl_type.is_primitive_type:
      cobalt_type = idl_primitive_type_to_cobalt(idl_type)
    elif idl_type.is_string_type:
      cobalt_type = idl_string_type_to_cobalt(idl_type)
    elif idl_type.is_callback_interface:
      cobalt_type = '::cobalt::script::CallbackInterfaceTraits<%s >' % (
          get_interface_name(idl_type))
    elif idl_type.is_interface_type:
      cobalt_type = 'scoped_refptr<%s>' % get_interface_name(idl_type)
    elif idl_type.is_union_type:
      cobalt_type = self.idl_union_type_to_cobalt(idl_type)
    elif idl_type.is_enum:
      cobalt_type = idl_type.name
    elif is_sequence_type(idl_type):
      cobalt_type = self.idl_sequence_type_to_cobalt(idl_type)
    elif idl_type.name == 'void':
      cobalt_type = 'void'
    elif is_date_type(idl_type):
      cobalt_type = 'base::Time'
    elif is_object_type(idl_type):
      cobalt_type = '::cobalt::script::ValueHandle'
    elif is_any_type(idl_type):
      cobalt_type = '::cobalt::script::ValueHandle'
    elif idl_type.is_dictionary:
      cobalt_type = get_interface_name(idl_type)
    elif is_promise_type(idl_type):
      cobalt_type = '::cobalt::script::NativePromise'

    assert cobalt_type, 'Unsupported idl_type %s' % idl_type

    if cobalt_type_is_optional(idl_type):
      cobalt_type = 'base::optional<%s >' % cobalt_type

    return cobalt_type

  def typed_object_to_cobalt_type(self, interface, typed_object):
    """Map type of IDL TypedObject to C++ type."""
    idl_type = self.resolve_typedef(typed_object.idl_type)
    if idl_type.is_callback_function:
      cobalt_type = interface.name + '::' + get_interface_name(idl_type)
    else:
      cobalt_type = self.idl_type_to_cobalt_type(idl_type)
    if getattr(typed_object, 'is_variadic', False):
      cobalt_type = 'std::vector<%s>' % cobalt_type
    return cobalt_type

  def typed_object_to_arg_type(self, interface, typed_object):
    """Map type of IDL TypedObject to C++ type that is a function argument."""
    base_type = self.typed_object_to_cobalt_type(interface, typed_object)

    idl_type = typed_object.idl_type
    if (idl_type.is_callback_function or idl_type.is_object_type or
        idl_type.is_callback_interface):
      return base_type + '*'
    if is_any_type(idl_type):
      return 'const ::cobalt::script::ScriptValue<%s>*' % base_type
    if idl_type.is_string_type or idl_type.is_interface_type:
      return 'const %s&' % base_type
    return base_type

  def argument_context(self, interface, argument):
    """Create template values for method/constructor arguments."""
    return {
        'idl_type_object':
            argument.idl_type,
        'name':
            argument.name,
        'type':
            self.typed_object_to_cobalt_type(interface, argument),
        'arg_type':
            self.typed_object_to_arg_type(interface, argument),
        'conversion_flags':
            get_conversion_flags(
                self.resolve_typedef(argument.idl_type),
                argument.extended_attributes),
        'is_optional':
            argument.is_optional,
        'is_variadic':
            argument.is_variadic,
        'default_value':
            idl_literal_to_cobalt_literal(argument.idl_type,
                                          argument.default_value)
            if argument.default_value else None,
    }

  def partial_context(self, interface, operation):
    """Create partial template values for generating bindings."""
    arguments = [
        self.argument_context(interface, a) for a in operation.arguments
    ]
    optional_arguments = get_optional_arguments(arguments)
    num_default_arguments = get_num_default_arguments(optional_arguments)
    return {
        'arguments':
            arguments,
        'non_optional_arguments':
            get_non_optional_arguments(arguments),
        'optional_arguments':
            optional_arguments,
        'num_default_arguments':
            num_default_arguments,
        'variadic_argument':
            get_variadic_argument(arguments),
        'has_non_default_optional_arguments':
            len(optional_arguments) > num_default_arguments,
    }

  def constructor_context(self, interface, constructor):
    """Create template values for generating constructor bindings."""
    context = {
        'call_with':
            interface.extended_attributes.get('ConstructorCallWith', None),
        'raises_exception': (interface.extended_attributes.get(
            'RaisesException', None) == 'Constructor'),
    }

    context.update(self.partial_context(interface, constructor))
    return context

  def method_context(self, interface, operation):
    """Create template values for generating method bindings."""
    context = {
        'idl_name':
            operation.name,
        'name':
            capitalize_function_name(operation.name),
        'type':
            self.typed_object_to_cobalt_type(interface, operation),
        'is_static':
            operation.is_static,
        'call_with':
            operation.extended_attributes.get('CallWith', None),
        'raises_exception':
            operation.extended_attributes.has_key('RaisesException'),
        'conditional':
            operation.extended_attributes.get('Conditional', None),
        'unsupported':
            'NotSupported' in operation.extended_attributes,
    }

    context.update(self.partial_context(interface, operation))
    return context

  def stringifier_context(self, interface):
    """Create template values for generating stringifier."""
    if not interface.stringifier:
      return None
    if interface.stringifier.attribute:
      cobalt_name = convert_to_cobalt_name(interface.stringifier.attribute.name)
    elif interface.stringifier.operation:
      cobalt_name = capitalize_function_name(
          interface.stringifier.operation.name)
    else:
      cobalt_name = 'AnonymousStringifier'
    return {
        'name': cobalt_name,
    }

  def special_method_context(self, interface, operation):
    """Create template values for getter and setter bindings."""
    if not operation or not operation.specials:
      return None

    assert operation.arguments
    is_indexed = str(operation.arguments[0].idl_type) == 'unsigned long'
    is_named = str(operation.arguments[0].idl_type) == 'DOMString'

    assert len(operation.specials) == 1
    special_type = operation.specials[0]
    assert special_type in ('getter', 'setter', 'deleter')

    function_suffix = {
        'getter': 'Getter',
        'setter': 'Setter',
        'deleter': 'Deleter',
    }

    if operation.name:
      cobalt_name = capitalize_function_name(operation.name)
    elif is_indexed:
      cobalt_name = 'AnonymousIndexed%s' % function_suffix[special_type]
    else:
      assert is_named
      cobalt_name = 'AnonymousNamed%s' % function_suffix[special_type]

    context = {
        'name':
            cobalt_name,
        'raises_exception':
            operation.extended_attributes.has_key('RaisesException'),
    }

    if special_type in ('getter', 'deleter'):
      context['type'] = self.typed_object_to_cobalt_type(interface, operation)
    else:
      value_argument = operation.arguments[1]
      context['type'] = self.typed_object_to_cobalt_type(
          interface, value_argument)
      context['conversion_flags'] = get_conversion_flags(
          self.resolve_typedef(value_argument.idl_type),
          value_argument.extended_attributes)

    return context

  def attribute_context(self, interface, attribute, definitions):
    """Create template values for attribute bindings."""
    cobalt_name = attribute.extended_attributes.get('ImplementedAs',
                                                    convert_to_cobalt_name(
                                                        attribute.name))
    context = {
        'idl_name':
            attribute.name,
        'getter_function_name':
            cobalt_name,
        'setter_function_name':
            'set_' + cobalt_name,
        'type':
            self.typed_object_to_cobalt_type(interface, attribute),
        'is_static':
            attribute.is_static,
        'is_read_only':
            attribute.is_read_only,
        'call_with':
            attribute.extended_attributes.get('CallWith', None),
        'raises_exception':
            attribute.extended_attributes.has_key('RaisesException'),
        'conversion_flags':
            get_conversion_flags(
                self.resolve_typedef(attribute.idl_type),
                attribute.extended_attributes),
        'conditional':
            attribute.extended_attributes.get('Conditional', None),
        'unsupported':
            'NotSupported' in attribute.extended_attributes,
    }
    forwarded_attribute_name = attribute.extended_attributes.get('PutForwards')
    if forwarded_attribute_name:
      assert attribute.idl_type.is_interface_type, (
          'PutForwards must be declared on a property of interface type.')
      assert attribute.is_read_only, (
          'PutForwards must be on a readonly attribute.')
      forwarded_interface = definitions.interfaces[get_interface_name(
          attribute.idl_type)]
      matching_attributes = [
          a for a in forwarded_interface.attributes
          if a.name == forwarded_attribute_name
      ]
      assert len(matching_attributes) == 1
      context['put_forwards'] = self.attribute_context(
          forwarded_interface, matching_attributes[0], definitions)
    context[
        'has_setter'] = not attribute.is_read_only or forwarded_attribute_name
    if is_constructor_attribute(attribute):
      context['is_constructor_attribute'] = True
      context['interface_name'] = get_interface_name(attribute.idl_type)
      # Blink's IDL parser uses the convention that attributes ending with
      # 'ConstructorConstructor' are for Named Constructors.
      context['is_named_constructor_attribute'] = (
          attribute.idl_type.name.endswith('ConstructorConstructor'))
    return context

  def enumeration_context(self, enumeration):
    """Create template values for IDL enumeration type bindings."""
    return {
        'enumeration_name':
            enumeration.name,
        'value_pairs': [(
            convert_to_cobalt_enumeration_value(enumeration.name, value),
            value,
        ) for value in enumeration.values],
    }

  def constant_context(self, constant):
    """Create template values for IDL constant bindings."""
    assert constant.idl_type.is_primitive_type, ('Only primitive types can be '
                                                 'declared as constants.')
    return {
        'name': convert_to_cobalt_constant_name(constant.name),
        'idl_name': constant.name,
        'value': constant.value,
        'can_use_compile_assert': constant.idl_type.is_integer_type,
        'unsupported': 'NotSupported' in constant.extended_attributes,
    }

  def get_method_contexts(self, expression_generator, interface):
    """Return a list of overload contexts for generating method bindings.

    The 'overloads' key of the overload_contexts will be the list of
    method_contexts that are overloaded to this name. In the case that
    a function is not overloaded, the length of this list will be one.

    Arguments:
        expression_generator: An ExpressionGenerator object.
        interface: an IdlInterface object
    Returns:
        [overload_contexts]
    """

    # Get the method contexts for all operations.
    methods = [
        self.method_context(interface, operation)
        for operation in interface.operations
        if operation.name
    ]

    # Create overload sets for static and non-static methods seperately.
    # Each item in the list is a pair of (name, [method_contexts]) where for
    # each method_context m in the list, m['name'] == name.
    static_method_overloads = method_overloads_by_name(
        [m for m in methods if m['is_static']])
    non_static_method_overloads = method_overloads_by_name(
        [m for m in methods if not m['is_static']])
    static_overload_contexts = get_overload_contexts(
        expression_generator,
        [contexts for _, contexts in static_method_overloads])
    non_static_overload_contexts = get_overload_contexts(
        expression_generator,
        [contexts for _, contexts in non_static_method_overloads])

    # Set is_static on each of these appropriately.
    for context in static_overload_contexts:
      context['is_static'] = True
    for context in non_static_overload_contexts:
      context['is_static'] = False

    # Append the lists and add the idl_name of the operation to the
    # top-level overload context.
    overload_contexts = static_overload_contexts + non_static_overload_contexts
    for context in overload_contexts:
      context['idl_name'] = context['overloads'][0]['idl_name']
      context['conditional'] = context['overloads'][0]['conditional']
      context['unsupported'] = context['overloads'][0]['unsupported']
      for overload in context['overloads']:
        assert context['conditional'] == overload['conditional'], (
            'All overloads must have the same conditional.')
      for overload in context['overloads']:
        assert context['unsupported'] == overload['unsupported'], (
            'All overloads must have the value for NotSupported.')

    return overload_contexts

  def get_constructor_context(self, expression_generator, interface):
    """Return an overload_context for generating constructor bindings.

    The 'overloads' key for the overloads_context will be a list of
    constructor_contexts representing all constructor overloads. In the
    case that the constructor is not overloaded, the length of this list
    will be one.
    The overload_context also has a 'length' key which can be used to
    specify the 'length' property for the constructor.

    Arguments:
        expression_generator: An ExpressionGenerator object.
        interface: An IdlInterface object.
    Returns:
        overload_context
    """

    constructors = [
        self.constructor_context(interface, constructor)
        for constructor in interface.constructors
    ]
    if not constructors:
      return None
    else:
      overload_contexts = get_overload_contexts(expression_generator,
                                                [constructors])
      assert len(overload_contexts) == 1, (
          'Expected exactly one overload context for constructor.')
      return overload_contexts[0]

  def get_dictionary_member_context(self, dictionary, dictionary_member):
    """Returns a jinja context for a dictionary member.

    Arguments:
        dictionary: An IdlDictionary object
        dictionary_member: An IdlDictionaryMember object.
    Returns:
      dictionary_member_context (dict)
    """
    return {
        'idl_type_object':
            dictionary_member.idl_type,
        'name':
            convert_to_cobalt_name(dictionary_member.name),
        'is_script_value':
            is_any_type(dictionary_member.idl_type),
        'idl_name':
            dictionary_member.name,
        'type':
            self.typed_object_to_cobalt_type(dictionary, dictionary_member),
        'arg_type':
            self.typed_object_to_arg_type(dictionary, dictionary_member),
        'conversion_flags':
            get_conversion_flags(
                self.resolve_typedef(dictionary_member.idl_type),
                dictionary_member.extended_attributes),
        'default_value':
            get_dictionary_default_value(dictionary_member.idl_type,
                                         dictionary_member.default_value,
                                         dictionary_member.name)
            if dictionary_member.default_value else None,
    }
