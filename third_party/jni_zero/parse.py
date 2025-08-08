# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import dataclasses
import os
import re
from typing import List
from typing import Optional

import java_types

_MODIFIER_KEYWORDS = (r'(?:(?:' + '|'.join([
    'abstract',
    'default',
    'final',
    'native',
    'private',
    'protected',
    'public',
    'static',
    'synchronized',
]) + r')\s+)*')


class ParseError(Exception):
  suffix = ''

  def __str__(self):
    return super().__str__() + self.suffix


@dataclasses.dataclass(order=True)
class ParsedNative:
  name: str
  signature: java_types.JavaSignature
  native_class_name: str
  static: bool = False


@dataclasses.dataclass(order=True)
class ParsedCalledByNative:
  java_class: java_types.JavaClass
  name: str
  signature: java_types.JavaSignature
  static: bool
  unchecked: bool = False


@dataclasses.dataclass(order=True)
class ParsedConstantField(object):
  name: str
  value: str


@dataclasses.dataclass
class ParsedFile:
  filename: str
  type_resolver: java_types.TypeResolver
  proxy_methods: List[ParsedNative]
  non_proxy_methods: List[ParsedNative]
  called_by_natives: List[ParsedCalledByNative]
  constant_fields: List[ParsedConstantField]
  proxy_interface: Optional[java_types.JavaClass] = None
  proxy_visibility: Optional[str] = None
  module_name: Optional[str] = None  # E.g. @NativeMethods("module_name")
  jni_namespace: Optional[str] = None  # E.g. @JNINamespace("content")


@dataclasses.dataclass
class _ParsedProxyNatives:
  interface_name: str
  visibility: str
  module_name: str
  methods: List[ParsedNative]


# Match single line comments, multiline comments, character literals, and
# double-quoted strings.
_COMMENT_REMOVER_REGEX = re.compile(
    r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
    re.DOTALL | re.MULTILINE)


def _remove_comments(contents):
  # We need to support both inline and block comments, and we need to handle
  # strings that contain '//' or '/*'.
  def replacer(match):
    # Replace matches that are comments with nothing; return literals/strings
    # unchanged.
    s = match.group(0)
    if s.startswith('/'):
      return ''
    else:
      return s

  return _COMMENT_REMOVER_REGEX.sub(replacer, contents)


# This will also break lines with comparison operators, but we don't care.
_GENERICS_REGEX = re.compile(r'<[^<>\n]*>')


def _remove_generics(value):
  """Strips Java generics from a string."""
  while True:
    ret = _GENERICS_REGEX.sub('', value)
    if len(ret) == len(value):
      return ret
    value = ret


_PACKAGE_REGEX = re.compile('^package\s+(\S+?);', flags=re.MULTILINE)


def _parse_package(contents):
  match = _PACKAGE_REGEX.search(contents)
  if not match:
    raise ParseError('Unable to find "package" line')
  return match.group(1)


_CLASSES_REGEX = re.compile(
    r'^(.*?)(?:\b(?:public|protected|private)?\b)\s*'
    r'(?:\b(?:static|abstract|final|sealed)\s+)*'
    r'\b(?:class|interface|enum)\s+(\w+?)\b[^"]*?$',
    flags=re.MULTILINE)


# Does not handle doubly-nested classes.
def _parse_java_classes(contents):
  package = _parse_package(contents).replace('.', '/')
  outer_class = None
  nested_classes = []
  for m in _CLASSES_REGEX.finditer(contents):
    preamble, class_name = m.groups()
    # Ignore annoations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue
    if outer_class is None:
      outer_class = java_types.JavaClass(f'{package}/{class_name}')
    else:
      nested_classes.append(outer_class.make_nested(class_name))

  if outer_class is None:
    raise ParseError('No classes found.')

  return outer_class, nested_classes


# Supports only @Foo and @Foo("value").
_ANNOTATION_REGEX = re.compile(r'@([\w.]+)(?:\(\s*"(.*?)\"\s*\))?\s*')


def _parse_annotations(value):
  annotations = {}
  last_idx = 0
  for m in _ANNOTATION_REGEX.finditer(value):
    annotations[m.group(1)] = m.group(2)
    last_idx = m.end()

  return annotations, value[last_idx:]


def _parse_type(type_resolver, value):
  """Parses a string into a JavaType."""
  annotations, value = _parse_annotations(value)
  array_dimensions = 0
  while value[-2:] == '[]':
    array_dimensions += 1
    value = value[:-2]

  if value in java_types.PRIMITIVES:
    primitive_name = value
    java_class = None
  else:
    primitive_name = None
    java_class = type_resolver.resolve(value)

  return java_types.JavaType(array_dimensions=array_dimensions,
                             primitive_name=primitive_name,
                             java_class=java_class,
                             annotations=annotations)


_FINAL_REGEX = re.compile(r'\bfinal\s')


def _parse_param_list(type_resolver, value) -> java_types.JavaParamList:
  if not value or value.isspace():
    return java_types.EMPTY_PARAM_LIST
  params = []
  value = _FINAL_REGEX.sub('', value)
  for param_str in value.split(','):
    param_str = param_str.strip()
    param_str, _, param_name = param_str.rpartition(' ')
    param_str = param_str.rstrip()

    # Handle varargs.
    if param_str.endswith('...'):
      param_str = param_str[:-3] + '[]'

    param_type = _parse_type(type_resolver, param_str)
    params.append(java_types.JavaParam(param_type, param_name))

  return java_types.JavaParamList(params)


_NATIVE_METHODS_INTERFACE_REGEX = re.compile(
    r'@NativeMethods(?:\(\s*"(?P<module_name>\w+)"\s*\))?[\S\s]+?'
    r'(?P<visibility>public)?\s*\binterface\s*'
    r'(?P<interface_name>\w*)\s*{(?P<interface_body>(\s*.*)+?\s*)}')

_PROXY_NATIVE_REGEX = re.compile(r'\s*(.*?)\s+(\w+)\((.*?)\);', flags=re.DOTALL)

_PUBLIC_REGEX = re.compile(r'\bpublic\s')


def _parse_proxy_natives(type_resolver, contents):
  matches = list(_NATIVE_METHODS_INTERFACE_REGEX.finditer(contents))
  if not matches:
    return None
  if len(matches) > 1:
    raise ParseError(
        'Multiple @NativeMethod interfaces in one class is not supported.')

  match = matches[0]
  ret = _ParsedProxyNatives(interface_name=match.group('interface_name'),
                            visibility=match.group('visibility'),
                            module_name=match.group('module_name'),
                            methods=[])
  interface_body = match.group('interface_body')

  for m in _PROXY_NATIVE_REGEX.finditer(interface_body):
    preamble, name, params_part = m.groups()
    preamble = _PUBLIC_REGEX.sub('', preamble)
    annotations, return_type_part = _parse_annotations(preamble)
    params = _parse_param_list(type_resolver, params_part)
    return_type = _parse_type(type_resolver, return_type_part)
    signature = java_types.JavaSignature.from_params(return_type, params)
    ret.methods.append(
        ParsedNative(
            name=name,
            signature=signature,
            native_class_name=annotations.get('NativeClassQualifiedName')))
  if not ret.methods:
    raise ParseError('Found no methods within @NativeMethod interface.')
  ret.methods.sort()
  return ret


_NON_PROXY_NATIVES_REGEX = re.compile(
    r'(@NativeClassQualifiedName'
    r'\(\"(?P<native_class_name>\S*?)\"\)\s+)?'
    r'(?P<qualifiers>\w+\s\w+|\w+|\s+)\s*native\s+'
    r'(?P<return_type>\S*)\s+'
    r'(?P<name>native\w+)\((?P<params>.*?)\);', re.DOTALL)


def _parse_non_proxy_natives(type_resolver, contents):
  ret = []
  for match in _NON_PROXY_NATIVES_REGEX.finditer(contents):
    name = match.group('name').replace('native', '')
    return_type = _parse_type(type_resolver, match.group('return_type'))
    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)
    native_class_name = match.group('native_class_name')
    static = 'static' in match.group('qualifiers')
    ret.append(
        ParsedNative(name=name,
                     signature=signature,
                     native_class_name=native_class_name,
                     static=static))
  ret.sort()
  return ret


# Regex to match a string like "@CalledByNative public void foo(int bar)".
_CALLED_BY_NATIVE_REGEX = re.compile(
    r'@CalledByNative((?P<Unchecked>(?:Unchecked)?|ForTesting))'
    r'(?:\("(?P<annotation>.*)"\))?'
    r'(?:\s+@\w+(?:\(.*\))?)*'  # Ignore any other annotations.
    r'\s+(?P<modifiers>' + _MODIFIER_KEYWORDS + r')' +
    r'(?:\s*@\w+)?'  # Ignore annotations in return types.
    r'\s*(?P<return_type>\S*?)'
    r'\s*(?P<name>\w+)'
    r'\s*\((?P<params>[^\)]*)\)')


def _parse_called_by_natives(type_resolver, contents):
  ret = []
  for match in _CALLED_BY_NATIVE_REGEX.finditer(contents):
    return_type_str = match.group('return_type')
    name = match.group('name')
    if return_type_str:
      return_type = _parse_type(type_resolver, return_type_str)
    else:
      return_type = java_types.VOID
      name = '<init>'

    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)
    inner_class_name = match.group('annotation')
    java_class = type_resolver.java_class
    if inner_class_name:
      java_class = java_class.make_nested(inner_class_name)

    ret.append(
        ParsedCalledByNative(java_class=java_class,
                             name=name,
                             signature=signature,
                             static='static' in match.group('modifiers'),
                             unchecked='Unchecked' in match.group('Unchecked')))

  # Check for any @CalledByNative occurrences that were not matched.
  unmatched_lines = _CALLED_BY_NATIVE_REGEX.sub('', contents).splitlines()
  for i, line in enumerate(unmatched_lines):
    if '@CalledByNative' in line:
      context = '\n'.join(unmatched_lines[i:i + 5])
      raise ParseError('Could not parse @CalledByNative method signature:\n' +
                       context)

  ret.sort()
  return ret


_IMPORT_REGEX = re.compile(r'^import\s+([^\s*]+);', flags=re.MULTILINE)
_IMPORT_CLASS_NAME_REGEX = re.compile(r'^(.*?)\.([A-Z].*)')


def _parse_imports(contents):
  # Regex skips static imports as well as wildcard imports.
  names = _IMPORT_REGEX.findall(contents)
  for name in names:
    m = _IMPORT_CLASS_NAME_REGEX.match(name)
    if m:
      package, class_name = m.groups()
      yield java_types.JavaClass(
          package.replace('.', '/') + '/' + class_name.replace('.', '$'))


_JNI_NAMESPACE_REGEX = re.compile('@JNINamespace\("(.*?)"\)')


def _parse_jni_namespace(contents):
  m = _JNI_NAMESPACE_REGEX.findall(contents)
  if not m:
    return ''
  if len(m) > 1:
    raise ParseError('Found multiple @JNINamespace annotations.')
  return m[0]


def _do_parse(filename, *, package_prefix):
  assert not filename.endswith('.kt'), (
      f'Found {filename}, but Kotlin is not supported by JNI generator.')
  with open(filename) as f:
    contents = f.read()
  contents = _remove_comments(contents)
  contents = _remove_generics(contents)

  outer_class, nested_classes = _parse_java_classes(contents)

  expected_name = os.path.splitext(os.path.basename(filename))[0]
  if outer_class.name != expected_name:
    raise ParseError(
        f'Found class "{outer_class.name}" but expected "{expected_name}".')

  if package_prefix:
    outer_class = outer_class.make_prefixed(package_prefix)
    nested_classes = [c.make_prefixed(package_prefix) for c in nested_classes]

  type_resolver = java_types.TypeResolver(outer_class)
  for java_class in _parse_imports(contents):
    type_resolver.add_import(java_class)
  for java_class in nested_classes:
    type_resolver.add_nested_class(java_class)

  parsed_proxy_natives = _parse_proxy_natives(type_resolver, contents)
  jni_namespace = _parse_jni_namespace(contents)

  non_proxy_methods = _parse_non_proxy_natives(type_resolver, contents)
  called_by_natives = _parse_called_by_natives(type_resolver, contents)

  ret = ParsedFile(filename=filename,
                   jni_namespace=jni_namespace,
                   type_resolver=type_resolver,
                   proxy_methods=[],
                   non_proxy_methods=non_proxy_methods,
                   called_by_natives=called_by_natives,
                   constant_fields=[])

  if parsed_proxy_natives:
    ret.module_name = parsed_proxy_natives.module_name
    ret.proxy_interface = outer_class.make_nested(
        parsed_proxy_natives.interface_name)
    ret.proxy_visibility = parsed_proxy_natives.visibility
    ret.proxy_methods = parsed_proxy_natives.methods

  return ret


def parse_java_file(filename, *, package_prefix=None):
  try:
    return _do_parse(filename, package_prefix=package_prefix)
  except ParseError as e:
    e.suffix = f' (when parsing {filename})'
    raise


_JAVAP_CLASS_REGEX = re.compile(r'\b(?:class|interface) (\S+)')
_JAVAP_FINAL_FIELD_REGEX = re.compile(
    r'^\s+public static final \S+ (.*?) = (\d+);', flags=re.MULTILINE)
_JAVAP_METHOD_REGEX = re.compile(
    rf'^\s*({_MODIFIER_KEYWORDS}).*?(\S+?)\(.*\n\s+descriptor: (.*)',
    flags=re.MULTILINE)


def parse_javap(filename, contents):
  contents = _remove_generics(contents)
  match = _JAVAP_CLASS_REGEX.search(contents)
  if not match:
    raise ParseError('Could not find java class in javap output')
  java_class = java_types.JavaClass(match.group(1).replace('.', '/'))
  type_resolver = java_types.TypeResolver(java_class)

  constant_fields = []
  for match in _JAVAP_FINAL_FIELD_REGEX.finditer(contents):
    name, value = match.groups()
    constant_fields.append(ParsedConstantField(name=name, value=value))
  constant_fields.sort()

  called_by_natives = []
  for match in _JAVAP_METHOD_REGEX.finditer(contents):
    modifiers, name, descriptor = match.groups()
    if name == java_class.full_name_with_dots:
      name = '<init>'
    signature = java_types.JavaSignature.from_descriptor(descriptor)

    called_by_natives.append(
        ParsedCalledByNative(java_class=java_class,
                             name=name,
                             signature=signature,
                             static='static' in modifiers))
  called_by_natives.sort()

  # Although javac will not allow multiple methods with no args and different
  # return types, Class.class has just that, and it breaks with our
  # name-mangling logic which assumes this cannot happen.
  if java_class.full_name_with_slashes == 'java/lang/Class':
    called_by_natives = [
        x for x in called_by_natives if 'TypeDescriptor' not in (
            x.signature.return_type.non_array_full_name_with_slashes)
    ]

  return ParsedFile(filename=filename,
                    type_resolver=type_resolver,
                    proxy_methods=[],
                    non_proxy_methods=[],
                    called_by_natives=called_by_natives,
                    constant_fields=constant_fields)
