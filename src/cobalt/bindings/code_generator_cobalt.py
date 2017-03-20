# Copyright 2014 Google Inc. All Rights Reserved.
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
"""Generate Cobalt bindings (.h and .cpp files).

Based on and borrows heavily from code_generator_v8.py which is used as part
of the bindings generation pipeline in Blink.
"""

import abc
from datetime import date
import os
import sys

import bootstrap_path  # pylint: disable=g-bad-import-order,unused-import

# Add blink's binding scripts to the path, so we can import
module_path, module_filename = os.path.split(os.path.realpath(__file__))
blink_script_dir = os.path.normpath(
    os.path.join(module_path, os.pardir, os.pardir, 'third_party', 'blink',
                 'Source'))
sys.path.append(blink_script_dir)

from bindings.scripts.code_generator_v8 import CodeGeneratorBase  # pylint: disable=g-import-not-at-top

# the code_generator_v8 module import in the line above updates sys.path, so
# that we can import the jinja2 in third_party.
import jinja2  # pylint: disable=g-bad-import-order

from bindings.scripts.idl_types import IdlType
from cobalt.bindings import contexts
from cobalt.bindings import path_generator
from cobalt.bindings.name_conversion import get_interface_name

module_path, module_filename = os.path.split(os.path.realpath(__file__))

# Track the cobalt directory, so we can use it for building relative paths.
cobalt_dir = os.path.normpath(os.path.join(module_path, os.pardir))

SHARED_TEMPLATES_DIR = os.path.abspath(os.path.join(module_path, 'templates'))


def initialize_jinja_env(cache_dir, templates_dir):
  """Initialize the Jinja2 environment."""
  assert os.path.isabs(templates_dir)
  assert os.path.isabs(SHARED_TEMPLATES_DIR)
  # Ensure that we are using the version of jinja that's checked in to
  # third_party.
  assert jinja2.__version__ == '2.7.1'
  jinja_env = jinja2.Environment(
      loader=jinja2.FileSystemLoader([templates_dir, SHARED_TEMPLATES_DIR]),
      extensions=['jinja2.ext.do'],  # do statement
      # Bytecode cache is not concurrency-safe unless pre-cached:
      # if pre-cached this is read-only, but writing creates a race condition.
      bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
      keep_trailing_newline=True,  # newline-terminate generated files
      lstrip_blocks=True,  # so can indent control flow tags
      trim_blocks=True)
  return jinja_env


def normalize_slashes(path):
  if os.path.sep == '\\':
    return path.replace('\\', '/')
  else:
    return path


def is_global_interface(interface):
  return (('PrimaryGlobal' in interface.extended_attributes) or
          ('Global' in interface.extended_attributes))


def get_indexed_special_operation(interface, special):
  special_operations = list(
      operation for operation in interface.operations
      if (special in operation.specials and operation.arguments and
          str(operation.arguments[0].idl_type) == 'unsigned long'))
  assert len(special_operations) <= 1, (
      'Multiple indexed %ss defined on interface: %s' % (special,
                                                         interface.name))
  return special_operations[0] if len(special_operations) else None


def get_indexed_property_getter(interface):
  getter_operation = get_indexed_special_operation(interface, 'getter')
  assert not getter_operation or len(getter_operation.arguments) == 1
  return getter_operation


def get_indexed_property_setter(interface):
  setter_operation = get_indexed_special_operation(interface, 'setter')
  assert not setter_operation or len(setter_operation.arguments) == 2
  return setter_operation


def get_indexed_property_deleter(interface):
  deleter_operation = get_indexed_special_operation(interface, 'deleter')
  assert not deleter_operation or len(deleter_operation.arguments) == 1
  return deleter_operation


def get_named_special_operation(interface, special):
  special_operations = list(
      operation for operation in interface.operations
      if (special in operation.specials and operation.arguments and
          str(operation.arguments[0].idl_type) == 'DOMString'))
  assert len(special_operations) <= 1, (
      'Multiple named %ss defined on interface: %s' % (special, interface.name))
  return special_operations[0] if len(special_operations) else None


def get_named_property_getter(interface):
  getter_operation = get_named_special_operation(interface, 'getter')
  assert not getter_operation or len(getter_operation.arguments) == 1
  return getter_operation


def get_named_property_setter(interface):
  setter_operation = get_named_special_operation(interface, 'setter')
  assert not setter_operation or len(setter_operation.arguments) == 2
  return setter_operation


def get_named_property_deleter(interface):
  deleter_operation = get_named_special_operation(interface, 'deleter')
  assert not deleter_operation or len(deleter_operation.arguments) == 1
  return deleter_operation


def get_interface_type_names_from_idl_types(idl_type_list):
  for idl_type in idl_type_list:
    if idl_type:
      if idl_type.is_interface_type:
        yield get_interface_name(idl_type)
      if idl_type.is_dictionary:
        yield idl_type.name
      elif idl_type.is_union_type:
        for interface_name in get_interface_type_names_from_idl_types(
            idl_type.member_types):
          yield interface_name


def get_interface_type_names_from_typed_objects(typed_object_list):
  for typed_object in typed_object_list:
    for interface_name in get_interface_type_names_from_idl_types(
        [typed_object.idl_type]):
      yield interface_name

    if hasattr(typed_object, 'arguments'):
      for interface_name in get_interface_type_names_from_typed_objects(
          typed_object.arguments):
        yield interface_name


def split_unsupported_properties(context_list):
  supported = []
  unsupported = []
  for context in context_list:
    if context['unsupported']:
      unsupported.append(context['idl_name'])
    else:
      supported.append(context)
  return supported, unsupported


class CodeGeneratorCobalt(CodeGeneratorBase):
  """Abstract Base code generator class for Cobalt.

  Concrete classes will provide an implementation for generating bindings for a
  specific JavaScript engine implementation.
  """
  __metaclass__ = abc.ABCMeta

  def __init__(self, interfaces_info, templates_dir, cache_dir, output_dir):
    CodeGeneratorBase.__init__(self, interfaces_info, cache_dir, output_dir)
    # CodeGeneratorBase inititalizes this with the v8 template path, so
    # reinitialize it with cobalt's template path

    # Whether the path is absolute or relative affects the cache file name. Use
    # the absolute path to ensure that we use the same path as was used when the
    # cache was prepopulated.
    self.jinja_env = initialize_jinja_env(cache_dir,
                                          os.path.abspath(templates_dir))
    self.path_builder = path_generator.PathBuilder(
        self.generated_file_prefix, interfaces_info, cobalt_dir, output_dir)

  @abc.abstractproperty
  def generated_file_prefix(self):
    """The prefix to prepend to all generated source files."""
    pass

  @abc.abstractproperty
  def expression_generator(self):
    """An instance that implements the ExpressionGenerator class."""
    pass

  def render_template(self, template_filename, template_context):
    template = self.jinja_env.get_template(template_filename)
    template_context.update(self.common_context(template))
    rendered_text = template.render(template_context)
    return rendered_text

  def generate_code_internal(self, definitions, definition_name):
    if definition_name in definitions.interfaces:
      return self.generate_interface_code(
          definitions, definition_name, definitions.interfaces[definition_name])
    if definition_name in definitions.dictionaries:
      return self.generate_dictionary_code(
          definitions, definition_name,
          definitions.dictionaries[definition_name])
    raise ValueError('%s is not in IDL definitions' % definition_name)

  def generate_interface_code(self, definitions, interface_name, interface):
    interface_info = self.interfaces_info[interface_name]
    # Select appropriate Jinja template and contents function
    if interface.is_callback:
      header_template_filename = 'callback-interface.h.template'
      cpp_template_filename = 'callback-interface.cc.template'
    elif interface.is_partial:
      raise NotImplementedError('Partial interfaces not implemented')
    else:
      header_template_filename = 'interface.h.template'
      cpp_template_filename = 'interface.cc.template'

    template_context = self.build_interface_context(interface, interface_info,
                                                    definitions)
    header_text = self.render_template(header_template_filename,
                                       template_context)
    cc_text = self.render_template(cpp_template_filename, template_context)
    header_path = self.path_builder.BindingsHeaderFullPath(interface_name)
    cc_path = self.path_builder.BindingsImplementationPath(interface_name)
    return ((header_path, header_text), (cc_path, cc_text),)

  def generate_dictionary_code(self, definitions, dictionary_name, dictionary):
    header_template_filename = 'dictionary.h.template'
    conversion_template_filename = 'dictionary-conversion.h.template'
    template_context = self.build_dictionary_context(dictionary, definitions)

    header_text = self.render_template(header_template_filename,
                                       template_context)
    conversion_text = self.render_template(conversion_template_filename,
                                           template_context)

    header_path = self.path_builder.DictionaryHeaderFullPath(dictionary_name)
    conversion_header_path = (
        self.path_builder.DictionaryConversionHeaderFullPath(dictionary_name))
    return ((header_path, header_text), (conversion_header_path,
                                         conversion_text),)

  def referenced_class_contexts(self,
                                interface_names,
                                implementation_only=False):
    """Returns a list of jinja contexts describing referenced C++ classes.

    Args:
      interface_names: A list of interfaces.
      implementation_only: Only include implemetation headers.
    Returns:
      list() of jinja contexts (python dicts) with information about C++ classes
      related to the interfaces in |interface_names|. dict has the following
      keys:
          fully_qualified_name: Fully qualified name of the class.
          include: Path to the header that defines the class.
          conditional: Symbol on which this interface is conditional compiled.
          is_callback_interface: True iff this is a callback interface.
    """
    referenced_classes = []
    # Iterate over it as a set to uniquify the list.
    for interface_name in set(interface_names):
      interface_info = self.interfaces_info[interface_name]
      is_dictionary = interface_info['is_dictionary']
      namespace = '::'.join(
          self.path_builder.NamespaceComponents(interface_name))
      conditional = interface_info['conditional']
      is_callback_interface = interface_name in IdlType.callback_interfaces

      # Information about the Cobalt implementation class.
      if is_dictionary:
        referenced_classes.append({
            'fully_qualified_name':
                '%s::%s' % (namespace, interface_name),
            'include':
                self.path_builder.BindingsHeaderIncludePath(interface_name),
            'conditional':
                conditional,
            'is_callback_interface':
                is_callback_interface,
        })
      else:
        referenced_classes.append({
            'fully_qualified_name':
                '%s::%s' % (namespace, interface_name),
            'include':
                self.path_builder.ImplementationHeaderPath(interface_name),
            'conditional':
                conditional,
            'is_callback_interface':
                is_callback_interface,
        })
        if not implementation_only:
          referenced_classes.append({
              'fully_qualified_name':
                  '%s::%s' % (namespace,
                              self.path_builder.BindingsClass(interface_name)),
              'include':
                  self.path_builder.BindingsHeaderIncludePath(interface_name),
              'conditional':
                  conditional,
              'is_callback_interface':
                  is_callback_interface,
          })
    return referenced_classes

  def common_context(self, template):
    """Shared stuff."""
    # Make sure extension is .py, not .pyc or .pyo, so doesn't depend on caching
    module_path_pyname = os.path.join(
        module_path, os.path.splitext(module_filename)[0] + '.py')

    # Ensure that posix forward slashes are used
    context = {
        'today':
            date.today(),
        'code_generator':
            normalize_slashes(os.path.relpath(module_path_pyname, cobalt_dir)),
        'template_path':
            normalize_slashes(os.path.relpath(template.filename, cobalt_dir)),
    }
    return context

  def build_dictionary_context(self, dictionary, definitions):
    context = {
        'class_name':
            dictionary.name,
        'header_file':
            self.path_builder.DictionaryHeaderIncludePath(dictionary.name),
        'members': [
            contexts.get_dictionary_member_context(dictionary, member)
            for member in dictionary.members
        ]
    }
    referenced_interface_names = set(
        get_interface_type_names_from_typed_objects(dictionary.members))
    referenced_class_contexts = self.referenced_class_contexts(
        referenced_interface_names, True)
    context['includes'] = sorted((interface['include']
                                  for interface in referenced_class_contexts))
    context['forward_declarations'] = sorted(
        referenced_class_contexts, key=lambda x: x['fully_qualified_name'])
    context['components'] = self.path_builder.NamespaceComponents(
        dictionary.name)
    return context

  def build_interface_context(self, interface, interface_info, definitions):
    context = {
        # Parameters used for template rendering.
        'today':
            date.today(),
        'binding_class':
            self.path_builder.BindingsClass(interface.name),
        'fully_qualified_binding_class':
            self.path_builder.FullBindingsClassName(interface.name),
        'header_file':
            self.path_builder.BindingsHeaderIncludePath(interface.name),
        'impl_class':
            interface.name,
        'fully_qualified_impl_class':
            self.path_builder.FullClassName(interface.name),
        'interface_name':
            interface.name,
        'is_global_interface':
            is_global_interface(interface),
        'has_interface_object': (
            'NoInterfaceObject' not in interface.extended_attributes),
        'conditional':
            interface.extended_attributes.get('Conditional', None),
        'add_opaque_roots':
            interface.extended_attributes.get('AddOpaqueRoots', None),
        'get_opaque_root':
            interface.extended_attributes.get('GetOpaqueRoot', None),
    }
    if is_global_interface(interface):
      # Global interface references all interfaces.
      referenced_interface_names = set(
          interface_name
          for interface_name in self.interfaces_info['all_interfaces']
          if not self.interfaces_info[interface_name]['unsupported'] and
          not self.interfaces_info[interface_name]['is_callback_interface'] and
          not self.interfaces_info[interface_name]['is_dictionary'])
      referenced_interface_names.update(IdlType.callback_interfaces)
    else:
      # Build the set of referenced interfaces from this interface's members.
      referenced_interface_names = set()
      referenced_interface_names.update(
          get_interface_type_names_from_typed_objects(interface.attributes))
      referenced_interface_names.update(
          get_interface_type_names_from_typed_objects(interface.operations))
      referenced_interface_names.update(
          get_interface_type_names_from_typed_objects(interface.constructors))
      referenced_interface_names.update(
          get_interface_type_names_from_typed_objects(
              definitions.callback_functions.values()))

    # Build the set of #includes in the header file. Try to keep this small
    # to avoid circular dependency problems.
    header_includes = set()
    if interface.parent:
      header_includes.add(
          self.path_builder.BindingsHeaderIncludePath(interface.parent))
      referenced_interface_names.add(interface.parent)
    header_includes.add(
        self.path_builder.ImplementationHeaderPath(interface.name))

    attributes = [
        contexts.attribute_context(interface, attribute, definitions)
        for attribute in interface.attributes
    ]
    constructor = contexts.get_constructor_context(self.expression_generator,
                                                   interface)
    methods = contexts.get_method_contexts(self.expression_generator, interface)
    constants = [contexts.constant_context(c) for c in interface.constants]

    # Get a list of all the unsupported property names, and remove the
    # unsupported ones from their respective lists
    attributes, unsupported_attribute_names = split_unsupported_properties(
        attributes)
    methods, unsupported_method_names = split_unsupported_properties(methods)
    constants, unsupported_constant_names = split_unsupported_properties(
        constants)

    # Build a set of all interfaces referenced by this interface.
    referenced_class_contexts = self.referenced_class_contexts(
        referenced_interface_names)

    all_interfaces = []
    for interface_name in referenced_interface_names:
      if (interface_name not in IdlType.callback_interfaces and
          not self.interfaces_info[interface_name]['unsupported'] and
          not self.interfaces_info[interface_name]['is_dictionary']):
        all_interfaces.append({
            'name': interface_name,
            'conditional': self.interfaces_info[interface_name]['conditional'],
        })

    context['implementation_includes'] = sorted(
        (interface['include'] for interface in referenced_class_contexts))
    context['header_includes'] = sorted(header_includes)
    context['attributes'] = [a for a in attributes if not a['is_static']]
    context['static_attributes'] = [a for a in attributes if a['is_static']]
    context['constants'] = constants
    context['constructor'] = constructor
    context['named_constructor'] = interface.extended_attributes.get(
        'NamedConstructor', None)
    context['interface'] = interface
    context['operations'] = [m for m in methods if not m['is_static']]
    context['static_operations'] = [m for m in methods if m['is_static']]
    context['unsupported_interface_properties'] = set(
        unsupported_attribute_names + unsupported_method_names +
        unsupported_constant_names)
    context['unsupported_constructor_properties'] = set(
        unsupported_constant_names)
    if interface.parent:
      context['parent_interface'] = self.path_builder.BindingsClass(
          interface.parent)
    context['is_exception_interface'] = interface.is_exception
    context['forward_declarations'] = sorted(
        referenced_class_contexts, key=lambda x: x['fully_qualified_name'])
    context['all_interfaces'] = sorted(all_interfaces, key=lambda x: x['name'])
    context['callback_functions'] = definitions.callback_functions.values()
    context['enumerations'] = [
        contexts.enumeration_context(enumeration)
        for enumeration in definitions.enumerations.values()
    ]
    context['components'] = self.path_builder.NamespaceComponents(
        interface.name)

    context['stringifier'] = contexts.stringifier_context(interface)
    context['indexed_property_getter'] = contexts.special_method_context(
        interface, get_indexed_property_getter(interface))
    context['indexed_property_setter'] = contexts.special_method_context(
        interface, get_indexed_property_setter(interface))
    context['indexed_property_deleter'] = contexts.special_method_context(
        interface, get_indexed_property_deleter(interface))
    context['named_property_getter'] = contexts.special_method_context(
        interface, get_named_property_getter(interface))
    context['named_property_setter'] = contexts.special_method_context(
        interface, get_named_property_setter(interface))
    context['named_property_deleter'] = contexts.special_method_context(
        interface, get_named_property_deleter(interface))

    context['supports_indexed_properties'] = (
        context['indexed_property_getter'] or
        context['indexed_property_setter'])
    context['supports_named_properties'] = (context['named_property_setter'] or
                                            context['named_property_getter'] or
                                            context['named_property_deleter'])

    return context


################################################################################


def main(argv):
  # If file itself executed, cache templates
  try:
    cache_dir = argv[1]
    templates_dir = argv[2]
    dummy_filename = argv[3]
  except IndexError:
    print 'Usage: %s CACHE_DIR TEMPLATES_DIR DUMMY_FILENAME' % argv[0]
    return 1

  # Delete all jinja2 .cache files, since they will get regenerated anyways.
  for filename in os.listdir(cache_dir):
    if os.path.splitext(filename)[1] == '.cache':
      # We expect that the only .cache files in this directory are for jinja2
      # and they have a __jinja2_ prefix.
      assert filename.startswith('__jinja2_')
      os.remove(os.path.join(cache_dir, filename))

  # Cache templates.
  # Whether the path is absolute or relative affects the cache file name. Use
  # the absolute path to ensure that the same path is used when we populate the
  # cache here and when it's read during code generation.
  jinja_env = initialize_jinja_env(cache_dir, os.path.abspath(templates_dir))
  template_filenames = [
      filename
      for filename in os.listdir(templates_dir)
      # Skip .svn, directories, etc.
      if filename.endswith(('.template'))
  ]
  assert template_filenames, 'Expected at least one template to be cached.'
  shared_template_filenames = [
      filename
      for filename in os.listdir(SHARED_TEMPLATES_DIR)
      # Skip .svn, directories, etc.
      if filename.endswith(('.template'))
  ]
  for template_filename in template_filenames + shared_template_filenames:
    jinja_env.get_template(template_filename)

  # Create a dummy file as output for the build system,
  # since filenames of individual cache files are unpredictable and opaque
  # (they are hashes of the template path, which varies based on environment)
  with open(dummy_filename, 'w') as dummy_file:
    pass  # |open| creates or touches the file


if __name__ == '__main__':
  sys.exit(main(sys.argv))
