#!/usr/bin/python

#
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
#
"""Flatten a set of IDLs.

Scan the specified directories for IDL files, filter out any ignored IDLs, and
then parse them. Flatten the parsed IDLs and write them out as a simple
representation of the members of each interface to the output directory.
"""

import argparse
from collections import defaultdict
import fnmatch
import os
import pickle
import sys
import textwrap


class FlattenedInterface(object):
  """Simplified representation of an IDL interface."""

  def __init__(self, name, operations, attributes, constants, constructors):
    """Initialize a FlattenedInterface object.

    Args:
      name: The name of the interface.
      operations: A list of strings representing the names of operations on the
          interface.
      attributes: A list of strings representing the names of attributes on the
          interface.
      constants: A list of strings representing the names of constants on the
          interface.
      constructors: A list of strings representing the names of constructors on
          the interface. For simplicity, the Interface Object is considered a
          constructor even if it's not callable.
    """
    self.name = name
    self.operations = operations
    self.attributes = attributes
    self.constants = constants
    self.constructors = constructors

  @classmethod
  def FromIdlInterface(cls, idl_interface):
    """Create a FlattenedInterface object from an IdlInterface object.

    Args:
      idl_interface: An idl_definitions.IdlInterface object.

    Returns:
      A new FlattenedInterface with the same properties as the IdlInterface.
    """
    operations = [o.name for o in idl_interface.operations]
    attributes = [a.name for a in idl_interface.attributes]
    constants = [c.name for c in idl_interface.constants]

    constructors = []

    if not idl_interface.extended_attributes.has_key('NoInterfaceObject'):
      constructors.append(idl_interface.name)
    if 'Constructor' in idl_interface.extended_attributes:
      constructors.append(idl_interface.name)
    if 'NamedConstructor' in idl_interface.extended_attributes:
      constructors.append(idl_interface.extended_attributes['NamedConstructor'])
    return cls(idl_interface.name, operations, attributes, constants,
               constructors)

  @classmethod
  def Difference(cls, lhs, rhs):
    """Determine all the properties in lhs that are not in rhs.

    Both |lhs| and |rhs| should describe the same interface. That is, they
    should have the same name.

    Args:
      lhs: A FlattenedInterface object.
      rhs: A FlattenedInterface object.

    Returns:
      A new FlattenedInterface object with only the properties that exist on
      |lhs| and do not exist on |rhs|.
    """
    assert lhs.name == rhs.name
    operations = list(set(lhs.operations).difference(rhs.operations))
    attributes = list(set(lhs.attributes).difference(rhs.attributes))
    constants = list(set(lhs.constants).difference(rhs.constants))
    constructors = list(set(lhs.constructors).difference(rhs.constructors))
    return cls(lhs.name, operations, attributes, constants, constructors)

  def IsEmpty(self):
    """Return True if this FlattenedInterface has no properties."""
    return not (self.operations or self.attributes or self.constants)

  @classmethod
  def SaveToPickle(cls, flattened_interfaces, output_file):
    """Pickle a list of FlattenedInterface objects.

    Args:
      flattened_interfaces: A list of FlattenedInterface instances.
      output_file: Path to which the pickle file will be written.
    """
    flattened_interfaces = list(flattened_interfaces)
    assert all((isinstance(i, cls) for i in flattened_interfaces))
    with open(output_file, 'w') as f:
      pickle.dump(flattened_interfaces, f)

  @classmethod
  def LoadFromPickle(cls, pickle_file):
    """Load a list of FlattenedInterface objects from a pickle file.

    Args:
      pickle_file: A pickle file created from the SaveToPickle function.
    Returns:
      A list of FlattenedInterface objects.
    """
    with open(pickle_file) as f:
      unpickled_list = pickle.load(f)
    assert all((isinstance(item, cls) for item in unpickled_list))
    return unpickled_list


def _GatherIDLFiles(dirname, ignore_patterns):
  """Walk all directories under |dirname| for files with the .idl extension."""

  def MatchesIgnorePattern(idl_path):
    return any(
        (fnmatch.fnmatch(idl_path, pattern) for pattern in ignore_patterns))

  for root, _, filenames in os.walk(dirname):
    idl_paths = (
        os.path.join(root, file) for file in fnmatch.filter(filenames, '*.idl'))
    for idl_path in idl_paths:
      if not MatchesIgnorePattern(idl_path):
        yield idl_path


def _FlattenInterfaces(idl_files):
  """Parse the list of idl_files and return a list of FlattenedInterfaces."""
  # Import idl_reader here rather than at the beginning of the file because the
  # module import path will be set based on an argument to this script.
  import idl_reader  # pylint: disable=g-import-not-at-top

  # Create an IdlReader that will parse the IDL and return the internal
  # representation of the interface.
  reader = idl_reader.IdlReader()

  # Map the interface name to an IdlInterface instance.
  interfaces = {}

  # Map an interface name to a list of its partial interfaces.
  partial_interfaces = defaultdict(list)

  # Map an interface name to the list of interfaces that it implements using
  # the 'implements' keyword.
  implemented_interfaces = defaultdict(list)

  # Parse each IDL in the list individually.
  for idl in idl_files:
    # Parse the IDL and get the idl_definitions.IdlDefinitions object.
    d = reader.read_idl_file(idl)

    # Track each interface that implements or is implemented by this interface.
    for i in d.implements:
      implemented_interfaces[i.left_interface].append(i.right_interface)

    for name, interface in d.interfaces.items():
      # Ignore callback interfaces.
      if interface.is_callback:
        continue

      if interface.is_partial:
        partial_interfaces[name].append(interface)
      else:
        assert name not in interfaces, ('Multiple IDL files found for '
                                        'interface: %s') % name
        interfaces[name] = interface

  # Merge partial interfaces into their main interface.
  for name, interface_list in partial_interfaces.items():
    for partial_interface in interface_list:
      interfaces[name].merge(partial_interface)

  all_implemented_interfaces = set()
  # Merge implemented interfaces into the interface that implements them.
  for lhs, interface_list in implemented_interfaces.items():
    for rhs in interface_list:
      interfaces[lhs].merge(interfaces[rhs])
      all_implemented_interfaces.add(rhs)

  # Sanity check that all interfaces that are on the right-hand-side also do
  # not have constructors.
  for interface in all_implemented_interfaces:
    assert not interfaces[interface].constructors

  # Convert the idl_definition.IdlInterface objects to FlattenedInterface
  # objects. Ignore interfaces that were on the right-hand-side of an implements
  # statement.
  return [
      FlattenedInterface.FromIdlInterface(interface)
      for name, interface in interfaces.items()
      if name not in all_implemented_interfaces
  ]


def main(argv):
  """Main function."""
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description=textwrap.dedent(__doc__))
  parser.add_argument(
      '-d',
      '--directory',
      action='append',
      required=True,
      dest='directories',
      help='Directories to search (recursively) for .idl files.')
  parser.add_argument(
      '-i',
      '--ignore',
      action='append',
      default=[],
      help='Ignore IDLs whose path contains the argument.')
  parser.add_argument(
      '-o',
      '--output_path',
      required=True,
      help='File into which flattened .idl files will be written.')
  parser.add_argument(
      '--blink_scripts_dir',
      required=True,
      help='Specify the directory from which blink\'s scripts should be imported.'
  )

  options = parser.parse_args(argv)

  if not os.path.isdir(options.blink_scripts_dir):
    raise RuntimeError('%s is not a directory' % options.blink_scripts_dir)

  # Set the script directory dynamically. This ensures that Blink's IDLs will
  # be loaded using Blink's IDL parsing scripts (and Cobalt IDLs will be loaded
  # using Cobalt's fork).
  # If the public interface to IdlReader or the IdlDefinitions class diverge too
  # much, we may have to refactor this into two functions.
  sys.path.append(options.blink_scripts_dir)

  idl_files = set()
  for directory in options.directories:
    idl_files.update(_GatherIDLFiles(directory, options.ignore))

  flattened_interfaces = _FlattenInterfaces(idl_files)

  FlattenedInterface.SaveToPickle(flattened_interfaces, options.output_path)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
