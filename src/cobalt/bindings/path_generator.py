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
"""Helper class for getting names and paths related to interfaces."""

import os

from cobalt.build.path_conversion import ConvertPath


def _NormalizeSlashes(path):
  if os.path.sep == '\\':
    return path.replace('\\', '/')
  else:
    return path


class PathBuilder(object):
  """Provides helper functions for getting paths related to an interface."""

  def __init__(self, engine_prefix, interfaces_info, interfaces_root,
               generated_root_directory):
    self.interfaces_root = _NormalizeSlashes(interfaces_root)
    self.generated_root = _NormalizeSlashes(generated_root_directory)
    self.engine_prefix = engine_prefix
    self.interfaces_info = interfaces_info

  def NamespaceComponents(self, interface_name):
    """Get the interface's namespace as a list of namespace components."""
    # Get the IDL filename relative to the cobalt directory, and split the
    # directory to get the list of namespace components.
    interface_info = self.interfaces_info[interface_name]
    idl_path = interface_info['full_path']

    idl_path = os.path.relpath(idl_path, self.interfaces_root)
    components = os.path.dirname(idl_path).split(os.sep)
    return [os.path.basename(self.interfaces_root)] + components

  def BindingsClass(self, interface_name):
    """Get the name of the generated bindings class."""
    return self.engine_prefix.capitalize() + interface_name

  def FullBindingsClassName(self, interface_name):
    """Get the fully qualified name of the generated bindings class."""
    components = self.NamespaceComponents(interface_name)
    return '::'.join(components + [self.BindingsClass(interface_name)])

  def FullClassName(self, interface_name):
    """Get the fully qualified name of the implementation class."""
    components = self.NamespaceComponents(interface_name)
    return '::'.join(components + [interface_name])

  def ImplementationHeaderPath(self, interface_name):
    """Get an #include path to the interface's implementation .h file."""
    interface_info = self.interfaces_info[interface_name]
    path = ConvertPath(
        interface_info['full_path'], forward_slashes=True, output_extension='h')
    return os.path.relpath(path, os.path.dirname(self.interfaces_root))

  def BindingsHeaderIncludePath(self, interface_name):
    """Get an #include path to the interface's generated .h file."""
    path = self.BindingsHeaderFullPath(interface_name)
    return os.path.relpath(path, self.generated_root)

  def BindingsHeaderFullPath(self, interface_name):
    """Get the full path to the interface's implementation .h file."""
    interface_info = self.interfaces_info[interface_name]
    return ConvertPath(
        interface_info['full_path'],
        forward_slashes=True,
        output_directory=self.generated_root,
        output_prefix='%s_' % self.engine_prefix,
        output_extension='h',
        base_directory=os.path.dirname(self.interfaces_root))

  def BindingsImplementationPath(self, interface_name):
    """Get the full path to the interface's implementation .cc file."""
    interface_info = self.interfaces_info[interface_name]
    return ConvertPath(
        interface_info['full_path'],
        forward_slashes=True,
        output_directory=self.generated_root,
        output_prefix='%s_' % self.engine_prefix,
        output_extension='cc',
        base_directory=os.path.dirname(self.interfaces_root))

  def DictionaryHeaderIncludePath(self, dictionary_name):
    """Get the #include path to the dictionary's header."""
    path = self.DictionaryHeaderFullPath(dictionary_name)
    return os.path.relpath(path, self.generated_root)

  def DictionaryHeaderFullPath(self, dictionary_name):
    """Get the full path to the dictionary's generated implementation header."""
    interface_info = self.interfaces_info[dictionary_name]
    return ConvertPath(
        interface_info['full_path'],
        forward_slashes=True,
        output_directory=self.generated_root,
        output_extension='h',
        base_directory=os.path.dirname(self.interfaces_root))

  def DictionaryConversionHeaderIncludePath(self, dictionary_name):
    """Get the #include path to the dictionary's conversion header."""
    path = self.BindingsHeaderFullPath(dictionary_name)
    return os.path.relpath(path, self.generated_root)

  def DictionaryConversionHeaderFullPath(self, dictionary_name):
    """Get the full path to the dictionary's conversion header."""
    interface_info = self.interfaces_info[dictionary_name]
    return ConvertPath(
        interface_info['full_path'],
        forward_slashes=True,
        output_directory=self.generated_root,
        output_prefix='%s_' % self.engine_prefix,
        output_extension='h',
        base_directory=os.path.dirname(self.interfaces_root))
