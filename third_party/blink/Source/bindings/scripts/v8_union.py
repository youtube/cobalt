# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import v8_utilities


UNION_H_INCLUDES = frozenset([
    'bindings/core/v8/ExceptionState.h',
    'bindings/core/v8/V8Binding.h',
    'platform/heap/Handle.h',
])

cpp_includes = set()
header_forward_decls = set()


def union_context(union_types, interfaces_info):
    return {
        'containers': [container_context(union_type, interfaces_info)
                       for union_type in union_types],
        'cpp_includes': sorted(cpp_includes),
        'header_forward_decls': sorted(header_forward_decls),
        'header_includes': sorted(UNION_H_INCLUDES),
    }


def container_context(union_type, interfaces_info):
    members = []

    # These variables refer to member contexts if the given union type has
    # corresponding types. They are used for V8 -> impl conversion.
    boolean_type = None
    dictionary_type = None
    interface_types = []
    numeric_type = None
    string_type = None
    for member in union_type.member_types:
        context = member_context(member, interfaces_info)
        members.append(context)
        if member.is_interface_type:
            interface_types.append(context)
        elif member.is_dictionary:
            if dictionary_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            dictionary_type = context
        elif member.base_type == 'boolean':
            if boolean_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            boolean_type = context
        elif member.is_numeric_type:
            if numeric_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            numeric_type = context
        elif member.is_string_type:
            if string_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            string_type = context
        else:
            raise Exception('%s is not supported as an union member.' % member.name)

    return {
        'boolean_type': boolean_type,
        'cpp_class': union_type.name,
        'dictionary_type': dictionary_type,
        'interface_types': interface_types,
        'members': members,
        'needs_trace': any(member['is_traceable'] for member in members),
        'numeric_type': numeric_type,
        'string_type': string_type,
    }


def member_context(member, interfaces_info):
    cpp_includes.update(member.includes_for_type)
    interface_info = interfaces_info.get(member.name, None)
    if interface_info:
        cpp_includes.update(interface_info.get('dependencies_include_paths', []))
        header_forward_decls.add(member.name)
    return {
        'cpp_name': v8_utilities.uncapitalize(member.name),
        'cpp_type': member.cpp_type_args(used_in_cpp_sequence=True),
        'cpp_local_type': member.cpp_type,
        'cpp_value_to_v8_value': member.cpp_value_to_v8_value(
            cpp_value='impl.getAs%s()' % member.name, isolate='isolate',
            creation_context='creationContext'),
        'is_traceable': (member.is_garbage_collected or
                         member.is_will_be_garbage_collected),
        'rvalue_cpp_type': member.cpp_type_args(used_as_rvalue_type=True),
        'specific_type_enum': 'SpecificType' + member.name,
        'type_name': member.name,
        'v8_value_to_local_cpp_value': member.v8_value_to_local_cpp_value(
            {}, 'v8Value', 'cppValue', needs_exception_state_for_string=True),
    }
