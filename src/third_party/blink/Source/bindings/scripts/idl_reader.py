# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Read an IDL file or complete IDL interface, producing an IdlDefinitions object.

Design doc:
http://www.chromium.org/developers/design-documents/idl-compiler#TOC-Front-end
"""

import os

import blink_idl_parser
from blink_idl_parser import BlinkIDLParser
from idl_definitions import IdlDefinitions
from idl_validator import EXTENDED_ATTRIBUTES_RELATIVE_PATH, IDLInvalidExtendedAttributeError, IDLExtendedAttributeValidator
from interface_dependency_resolver import InterfaceDependencyResolver
from utilities import idl_filename_to_component, idl_filename_to_interface_name


class IdlReader(object):
    def __init__(self, extend_attributes_filename=None, interfaces_info=None, outputdir=''):
        self.extended_attribute_validator = None
        if extend_attributes_filename:
          self.extended_attribute_validator = IDLExtendedAttributeValidator(extend_attributes_filename)
        self.interfaces_info = interfaces_info

        if interfaces_info:
            self.interface_dependency_resolver = InterfaceDependencyResolver(interfaces_info, self)
        else:
            self.interface_dependency_resolver = None

        self.parser = BlinkIDLParser(outputdir=outputdir)

    def read_idl_definitions(self, idl_filename):
        """Returns a dictionary whose key is component and value is an IdlDefinitions object for an IDL file, including all dependencies."""
        definitions = self.read_idl_file(idl_filename)
        component = idl_filename_to_component(idl_filename)

        if not self.interface_dependency_resolver:
            return {component: definitions}

        # This definitions should have a dictionary. No need to resolve any
        # dependencies.
        if not definitions.interfaces:
            return {component: definitions}

        return self.interface_dependency_resolver.resolve_dependencies(definitions, component)

    def read_idl_file(self, idl_filename):
        """Returns an IdlDefinitions object for an IDL file, without any dependencies.

        The IdlDefinitions object is guaranteed to contain a single
        IdlInterface; it may also contain other definitions, such as
        callback functions and enumerations."""
        ast = blink_idl_parser.parse_file(self.parser, idl_filename)
        if not ast:
            raise Exception('Failed to parse %s' % idl_filename)
        if ast.GetProperty('ERRORS'):
            raise Exception('Encountered %d error(s) parsing %s' % (
                ast.GetProperty('ERRORS'), idl_filename))
        idl_interface_name = idl_filename_to_interface_name(idl_filename)
        definitions = IdlDefinitions(idl_interface_name, ast)

        # Validate file contents with filename convention
        # The Blink IDL filenaming convention is that the file
        # <definition_name>.idl MUST contain exactly 1 definition
        # (interface, dictionary or exception), and the definition name must
        # agree with the file's basename, unless it is a partial definition.
        # (e.g., 'partial interface Foo' can be in FooBar.idl).
        targets = (definitions.interfaces.values() +
                   definitions.dictionaries.values())
        number_of_targets = len(targets)
        if number_of_targets != 1:
            raise Exception(
                'Expected exactly 1 definition in file {0}, but found {1}'
                .format(idl_filename, number_of_targets))
        target = targets[0]
        if not target.is_partial and target.name != idl_interface_name:
            if target.name.lower() == idl_interface_name.lower():
                # Names differ only by capitalization. This could indicate a
                # problem with the special tokens in the snake_case->TitleCase
                # conversion in
                raise Exception(
                    '"Unexpected capitalization of definition name "{0}" for IDL file name "{1}". '
                    'Does a special token need to be added to '
                    'idl_filename_to_interface_name in utilities.py?'
                    .format(target.name, os.path.basename(idl_filename)))
            else:
                raise Exception(
                    'Definition name "{0}" disagrees with IDL file name "{1}".'
                    .format(target.name, os.path.basename(idl_filename)))

        # Validate extended attributes
        if not self.extended_attribute_validator:
            return definitions

        try:
            self.extended_attribute_validator.validate_extended_attributes(definitions)
        except IDLInvalidExtendedAttributeError as error:
            raise IDLInvalidExtendedAttributeError("""
IDL ATTRIBUTE ERROR in file:
%s:
    %s
If you want to add a new IDL extended attribute, please add it to:
    %s
and add an explanation to the Blink IDL documentation at:
    http://www.chromium.org/blink/webidl/blink-idl-extended-attributes
    """ % (idl_filename, str(error), EXTENDED_ATTRIBUTES_RELATIVE_PATH))

        return definitions
