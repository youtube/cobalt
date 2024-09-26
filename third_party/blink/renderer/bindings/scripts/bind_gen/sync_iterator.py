# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import native_value_tag
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_utils import collect_forward_decls_and_include_headers
from .codegen_utils import component_export
from .package_initializer import package_initializer
from .task_queue import TaskQueue

# Sync iterators are mostly the same as platform objects, so this module uses
# the implementation of IDL interface.
from .interface import generate_class_like


def make_constructors(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    decls = ListNode([
        CxxFuncDefNode(name=cg_context.class_name,
                       arg_decls=["IterationSourceBase* source", "Kind kind"],
                       return_type="",
                       explicit=True,
                       member_initializer_list=[
                           "${base_class_name}(source, kind)",
                       ]),
        CxxFuncDeclNode(name="~SyncIterator",
                        arg_decls=[],
                        return_type="",
                        override=True,
                        default=True),
    ])

    return decls, None


def generate_sync_iterator_blink_impl_class(sync_iterator=None,
                                            api_component=None,
                                            for_testing=None,
                                            header_blink_ns=None,
                                            source_blink_ns=None):
    assert sync_iterator is not None
    assert api_component is not None
    assert for_testing is not None
    assert header_blink_ns is not None
    assert source_blink_ns is not None

    # SyncIterator<InterfaceClass> (ScriptWrappable) definition
    cg_context = CodeGenContext(sync_iterator=sync_iterator,
                                class_name=blink_class_name(sync_iterator),
                                base_class_name="bindings::SyncIteratorBase")
    class_def = CxxClassDefNode(cg_context.class_name,
                                base_class_names=[cg_context.base_class_name],
                                template_params=[],
                                final=True,
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    key_type = (sync_iterator.key_type.unwrap(
        typedef=True) if sync_iterator.key_type else None)
    value_type = sync_iterator.value_type.unwrap(typedef=True)
    key_value_type_list = tuple(filter(None, [key_type, value_type]))

    (header_forward_decls, header_include_headers, source_forward_decls,
     source_include_headers
     ) = collect_forward_decls_and_include_headers(key_value_type_list)
    class_def.accumulate(
        CodeGenAccumulator.require_class_decls(
            set.union(header_forward_decls, source_forward_decls)))
    headers = set([
        "third_party/blink/renderer/platform/bindings/sync_iterator_base.h",
    ])
    headers.update(header_include_headers)
    for idl_type in key_value_type_list:
        if idl_type.is_numeric or idl_type.is_string or idl_type.is_nullable:
            headers.add(
                "third_party/blink/renderer/bindings/core/v8/idl_types.h")
    class_def.accumulate(CodeGenAccumulator.require_include_headers(headers))

    ctor_decls, ctor_defs = make_constructors(cg_context)

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    class_def.top_section.append(TextNode("DEFINE_WRAPPERTYPEINFO();"))

    class_def.public_section.append(
        TextNode("using IDLKeyType = {};".format(
            native_value_tag(key_type) if key_type else "void")))
    class_def.public_section.append(
        TextNode("using IDLValueType = {};".format(
            native_value_tag(value_type))))
    class_def.public_section.append(
        TextNode("using KeyType = {};".format(
            blink_type_info(key_type).value_t if key_type else "void")))
    class_def.public_section.append(
        TextNode("using ValueType = {};".format(
            blink_type_info(value_type).value_t)))
    class_def.public_section.append(EmptyNode())

    class_def.public_section.append(ctor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(ctor_defs)
    source_blink_ns.body.append(EmptyNode())


def generate_sync_iterator(interface_identifier):
    assert isinstance(interface_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    interface = web_idl_database.find(interface_identifier)
    sync_iterator = interface.sync_iterator

    generate_class_like(sync_iterator,
                        generate_sync_iterator_blink_impl_class_callback=(
                            generate_sync_iterator_blink_impl_class))


def generate_sync_iterators(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for interface in web_idl_database.interfaces:
        if not interface.sync_iterator:
            continue
        task_queue.post_task(generate_sync_iterator, interface.identifier)
