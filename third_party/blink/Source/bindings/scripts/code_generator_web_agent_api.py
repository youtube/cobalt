# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Generates Web Agent API bindings.

The Web Agent API bindings provide a stable, IDL-generated interface for the
Web Agents.

The Web Agents are the high-level services like Autofill,
Autocomplete, Translate, Distiller, Phishing Detector, and others. Web Agents
typically want to introspec the document and rendering infromation to implement
browser features.

The bindings are meant to be as simple and as ephemeral as possible, mostly just
wrapping existing DOM classes. Their primary goal is to avoid leaking the actual
DOM classes to the Web Agents layer.
"""

import os
import posixpath

from code_generator import CodeGeneratorBase, render_template
# TODO(dglazkov): Move TypedefResolver to code_generator.py
from code_generator_v8 import TypedefResolver
from name_style_converter import NameStyleConverter

MODULE_PYNAME = os.path.splitext(os.path.basename(__file__))[0] + '.py'

STRING_INCLUDE_PATH = 'wtf/text/WTFString.h'
WEB_AGENT_API_IDL_ATTRIBUTE = 'WebAgentAPI'


def interface_context(idl_interface, type_resolver):
    builder = InterfaceContextBuilder(MODULE_PYNAME, type_resolver)
    builder.set_class_name(idl_interface.name)
    builder.set_inheritance(idl_interface.parent)

    for idl_attribute in idl_interface.attributes:
        builder.add_attribute(idl_attribute)

    for idl_operation in idl_interface.operations:
        builder.add_operation(idl_operation)

    return builder.build()


class TypeResolver(object):
    """Resolves Web IDL types into corresponding C++ types and include paths
       to the generated and existing files."""

    def __init__(self, interfaces_info):
        self.interfaces_info = interfaces_info

    def includes_from_interface(self, interface_name):
        interface_info = self.interfaces_info.get(interface_name)
        if interface_info is None:
            raise KeyError('Unknown interface "%s".' % interface_name)
        return set([interface_info['include_path']])

    def _includes_from_type(self, idl_type):
        if idl_type.is_void:
            return set()
        if idl_type.is_primitive_type:
            return set()
        if idl_type.is_string_type:
            return set([STRING_INCLUDE_PATH])

        # TODO(dglazkov): Handle complex/weird types.
        return self.includes_from_interface(idl_type.base_type)

    def includes_from_definition(self, idl_definition):
        return self._includes_from_type(idl_definition.idl_type)

    def type_from_definition(self, idl_definition):
        # TODO(dglazkov): The output of this method must be a reasonable C++
        # type that can be used directly in the jinja2 template.
        return idl_definition.idl_type.base_type

    def base_class_includes(self):
        return set(['platform/heap/Handle.h'])


class InterfaceContextBuilder(object):
    def __init__(self, code_generator, type_resolver):
        self.result = {'code_generator': code_generator}
        self.type_resolver = type_resolver

    def set_class_name(self, class_name):
        converter = NameStyleConverter(class_name)
        self.result['class_name'] = converter.to_all_cases()
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_interface(class_name))

    def set_inheritance(self, base_interface):
        if base_interface is None:
            self._ensure_set('header_includes').update(
                self.type_resolver.base_class_includes())
            return
        self.result['base_class'] = base_interface
        self._ensure_set('header_includes').update(
            self.type_resolver.includes_from_interface(base_interface))

    def _ensure_set(self, name):
        return self.result.setdefault(name, set())

    def _ensure_list(self, name):
        return self.result.setdefault(name, [])

    def add_attribute(self, idl_attribute):
        self._ensure_list('attributes').append(
            self.create_attribute(idl_attribute))
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_definition(idl_attribute))

    def add_operation(self, idl_operation):
        if not idl_operation.name:
            return
        self._ensure_list('methods').append(
            self.create_method(idl_operation))
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_definition(idl_operation))

    def create_method(self, idl_operation):
        name = idl_operation.name
        return_type = self.type_resolver.type_from_definition(idl_operation)
        return {
            'name': name,
            'return_type': return_type
        }

    def create_attribute(self, idl_attribute):
        name = idl_attribute.name
        return_type = self.type_resolver.type_from_definition(idl_attribute)
        return {
            'name': name,
            'return_type': return_type
        }

    def build(self):
        return self.result


class CodeGeneratorWebAgentAPI(CodeGeneratorBase):
    def __init__(self, info_provider, cache_dir, output_dir):
        CodeGeneratorBase.__init__(self, MODULE_PYNAME, info_provider,
                                   cache_dir, output_dir)
        self.type_resolver = TypeResolver(info_provider.interfaces_info)
        self.typedef_resolver = TypedefResolver(info_provider)

    def get_template(self, file_extension):
        template_filename = 'web_agent_api_interface.%s.tmpl' % file_extension
        return self.jinja_env.get_template(template_filename)

    def generate_file(self, template_context, file_extension):
        template = self.get_template(file_extension)
        path = posixpath.join(
            self.output_dir,
            '%s.%s' % (template_context['class_name']['snake_case'],
                       file_extension))
        text = render_template(template, template_context)
        return (path, text)

    def generate_interface_code(self, interface):
        # TODO(dglazkov): Implement callback interfaces.
        # TODO(dglazkov): Make sure partial interfaces are handled.
        if interface.is_callback or interface.is_partial:
            raise ValueError('Partial or callback interfaces are not supported')

        template_context = interface_context(interface, self.type_resolver)

        return (
            self.generate_file(template_context, 'h'),
            self.generate_file(template_context, 'cc')
        )

    def generate_code(self, definitions, definition_name):
        self.typedef_resolver.resolve(definitions, definition_name)

        # TODO(dglazkov): Implement dictionaries
        if definition_name not in definitions.interfaces:
            return None

        interface = definitions.interfaces[definition_name]
        if WEB_AGENT_API_IDL_ATTRIBUTE not in interface.extended_attributes:
            return None

        return self.generate_interface_code(interface)
