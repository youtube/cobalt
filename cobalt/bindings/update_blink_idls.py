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

"""Generate IDL files for interfaces that are not implemented in Cobalt.

This will do a shallow clone of the chromium repository and gather the set of
all IDL files. These IDLs will be flattened so that there is one IDL per
interface. That is, no IDLs will have partial interfaces nor will any IDL use
the "implements" keyword.
These IDL files are diff'd against the same flattened IDL files from Cobalt, and
new IDL files representing the unimplemented interfaces and properties will be
written to the output directory.
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap

from flatten_idls import FlattenedInterface

_SCRIPT_DIR = os.path.dirname(__file__)
_CHROMIUM_REPOSITORY_URL = 'https://chromium.googlesource.com/chromium/src'
_UNIMPLEMENTED_INTERFACE_TEMPLATE = '[NotSupported] interface {} {{}};\n'


def _ClobberDirectory(dirname):
  """Delete the directory if it exists, and create it if it does not."""
  if os.path.exists(dirname):
    shutil.rmtree(dirname)
  os.makedirs(dirname)


def _CloneChromium(branch, destination_dir):
  """Clone the specified branch of Chromium to the specified directory.

  Only do a shallow clone since we don't care about the full history.

  Args:
    branch: Name of a branch in Chromium's git repository.
    destination_dir: Directory into which Chromium repository will be cloned.
  """
  clone_command = ['git', 'clone', '--depth', '1', '--branch', branch,
                   _CHROMIUM_REPOSITORY_URL, '.']
  subprocess.check_call(clone_command, cwd=destination_dir)


def _LoadInterfaces(interfaces_pickle):
  """Load the interfaces from a pickle and return a name->interface dict."""
  interfaces = FlattenedInterface.LoadFromPickle(interfaces_pickle)
  return dict(((interface.name, interface) for interface in interfaces))


def _WriteUnsupportedInterfaceIDL(interface_name, output_dir):
  """Write out InterfaceName.idl for an unsupported interface.

  The interface will have the [NotSupported] attribute set.

  Args:
    interface_name: The name of the interface whose IDL file will be created.
    output_dir: Directory into which the generated IDL file will be written.
  """
  output_idl_filename = os.path.join(output_dir, interface_name) + '.idl'
  with open(output_idl_filename, 'w') as f:
    f.write(_UNIMPLEMENTED_INTERFACE_TEMPLATE.format(interface_name))


def _WritePartiallySupportedInterfaceIDL(interface, output_dir):
  """Write out InterfaceName_unsupported.idl.

  Unsupported interface members will have the [NotSupported] extended attribute.

  Args:
    interface: A FlattenedInterface object.
    output_dir: Directory into which the generated IDL file will be written.
  """
  output_idl_filename = os.path.join(output_dir,
                                     interface.name) + '_unsupported.idl'
  with open(output_idl_filename, 'w') as f:
    f.write('partial interface %s {\n' % interface.name)
    for c in interface.constants:
      # Type doesn't matter so use long
      f.write('  [NotSupported] const long %s;\n' % c)
    for a in interface.attributes:
      f.write('  [NotSupported] attribute long %s;\n' % a)
    for o in interface.operations:
      f.write('  [NotSupported] void %s();\n' % o)
    f.write('}\n')


def main(argv):
  """Main function."""
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description=textwrap.dedent(__doc__))
  parser.add_argument(
      '--branch',
      help='Branch or tag to fetch from the chromium repository. '
      'This will be passed to git clone\'s -b parameter.')
  parser.add_argument(
      '--chromium_dir',
      help='Directory containing a chromium repository. If the --branch '
      'argument is set, the directory will be clobbered and the specified '
      'branch will be cloned into this directory.')
  parser.add_argument('--output_dir',
                      required=True,
                      help='Directory into which IDL files will be placed. '
                      'The current contents will be clobbered.')

  options = parser.parse_args(argv)
  logging_format = '%(asctime)s %(levelname)-8s %(message)s'
  logging.basicConfig(level=logging.INFO,
                      format=logging_format,
                      datefmt='%m-%d %H:%M')

  temp_dir = tempfile.mkdtemp()

  try:
    if not options.chromium_dir:
      chromium_dir = os.path.join(temp_dir, 'chromium')
      os.makedirs(chromium_dir)
    else:
      chromium_dir = options.chromium_dir

    if options.branch:
      logging.info('Cloning chromium branch %s into %s', options.branch,
                   chromium_dir)
      _ClobberDirectory(chromium_dir)
      # Git clone into temp_dir
      _CloneChromium(options.branch, chromium_dir)
    else:
      assert os.path.isdir(chromium_dir)

    # Gather the blink IDLs
    logging.info('Gathering blink IDLs.')
    blink_pickle_file = os.path.join(temp_dir, 'blink_idl.pickle')
    subprocess.check_call(
        ['python', 'flatten_idls.py', '--directory', os.path.join(
            chromium_dir, 'third_party/WebKit/Source/core'), '--directory',
         os.path.join(chromium_dir, 'third_party/WebKit/Source/modules'),
         '--ignore', '*/InspectorInstrumentation.idl', '--blink_scripts_dir',
         os.path.join(chromium_dir,
                      'third_party/WebKit/Source/bindings/scripts'),
         '--output_path', blink_pickle_file])

    # Gather Cobalt's IDLs
    logging.info('Gathering Cobalt IDLs.')
    cobalt_root = os.path.join(_SCRIPT_DIR, '../../../')
    cobalt_pickle_file = os.path.join(temp_dir, 'cobalt_idl.pickle')
    subprocess.check_call(
        ['python', 'flatten_idls.py', '--directory', os.path.join(
            cobalt_root, 'cobalt'), '--ignore', '*/cobalt/bindings/*',
         '--blink_scripts_dir', os.path.join(
             cobalt_root, 'third_party/blink/Source/bindings/scripts'),
         '--output_path', cobalt_pickle_file])

    # Unpickle the files.
    blink_interfaces = _LoadInterfaces(blink_pickle_file)
    cobalt_interfaces = _LoadInterfaces(cobalt_pickle_file)

    blink_interface_names = set(blink_interfaces.keys())
    cobalt_interface_names = set(cobalt_interfaces.keys())

    _ClobberDirectory(options.output_dir)

    # Write out an IDL for unimplemented interfaces. For simplicity, an IDL will
    # be written out for each named constructor as well.
    for interface_name in blink_interface_names.difference(
        cobalt_interface_names):
      interface = blink_interfaces[interface_name]
      for constructor in interface.constructors:
        _WriteUnsupportedInterfaceIDL(constructor, options.output_dir)

    # Get all the cobalt interfaces that are also in blink
    for interface_name in cobalt_interface_names.intersection(
        blink_interface_names):
      unimplemented = FlattenedInterface.Difference(
          blink_interfaces[interface_name], cobalt_interfaces[interface_name])
      if not unimplemented.IsEmpty():
        _WritePartiallySupportedInterfaceIDL(unimplemented, options.output_dir)
  finally:
    shutil.rmtree(temp_dir)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
