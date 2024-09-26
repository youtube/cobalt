# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_default_value_expr
from .blink_v8_bridge import native_value_tag
from .code_node import EmptyNode
from .code_node import FormatNode
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxNamespaceNode
from .code_node_cxx import CxxUnlikelyIfNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_expr import expr_from_exposure
from .codegen_format import format_template as _format
from .codegen_utils import collect_forward_decls_and_include_headers
from .codegen_utils import component_export
from .codegen_utils import component_export_header
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .package_initializer import package_initializer
from .path_manager import PathManager
from .task_queue import TaskQueue


class _DictionaryMember(object):
    """
    _DictionaryMember represents the properties that the code generator
    directly needs while web_idl.DictionaryMember represents properties of IDL
    dictionary member independent from ECMAScript binding.  _DictionaryMember
    is specific to not only ECMAScript binding but also Blink implementation
    of IDL dictionary.
    """

    # Map from Blink member type to presence expression.
    _MEMBER_TYPE_TO_PRESENCE_EXPR = {
        "ScriptPromise": "!{}.IsEmpty()",
        "ScriptValue": "!{}.IsEmpty()",
    }

    def __init__(self, dict_member):
        assert isinstance(dict_member, web_idl.DictionaryMember)

        self._identifier = dict_member.identifier
        self._base_name = (
            dict_member.code_generator_info.property_implemented_as
            or dict_member.identifier)

        self._api_has = name_style.api_func("has", self._base_name)
        self._api_get = name_style.api_func(self._base_name)
        self._api_get_or = name_style.api_func("get", self._base_name, "or")
        self._api_set = name_style.api_func("set", self._base_name)
        # C++ data member that shows the presence of the dictionary member.
        self._presence_var = name_style.member_var("has", self._base_name)
        # C++ data member that holds the value of the dictionary member.
        self._value_var = name_style.member_var("member", self._base_name)

        # Migration adapters
        self._api_has_non_null = name_style.api_func("has", self._base_name,
                                                     "non_null")
        self._api_get_non_null = name_style.api_func(self._base_name,
                                                     "non_null")

        self._idl_type = dict_member.idl_type
        self._type_info = blink_type_info(self._idl_type)
        self._is_required = dict_member.is_required
        if dict_member.default_value:
            self._default_expr = make_default_value_expr(
                self._idl_type, dict_member.default_value)
        else:
            self._default_expr = None

        self._exposure = dict_member.exposure

    @property
    def identifier(self):
        return self._identifier

    @property
    def api_has(self):
        return self._api_has

    @property
    def api_get(self):
        return self._api_get

    @property
    def api_get_or(self):
        return self._api_get_or

    @property
    def api_set(self):
        return self._api_set

    @property
    def api_has_non_null(self):
        return self._api_has_non_null

    @property
    def api_get_non_null(self):
        return self._api_get_non_null

    @property
    def presence_var(self):
        return self._presence_var

    @property
    def value_var(self):
        return self._value_var

    @property
    def idl_type(self):
        return self._idl_type

    @property
    def type_info(self):
        return self._type_info

    @property
    def is_required(self):
        return self._is_required

    @property
    def presence_expr(self):
        if self.is_always_present:
            return "true"
        expr = self._MEMBER_TYPE_TO_PRESENCE_EXPR.get(self.type_info.member_t)
        if expr:
            return expr.format(self.value_var)
        return self.presence_var

    @property
    def does_use_presence_var(self):
        return not (
            self.is_always_present
            or self.type_info.member_t in self._MEMBER_TYPE_TO_PRESENCE_EXPR)

    @property
    def is_always_present(self):
        return bool(self.is_required or self._default_expr)

    @property
    def initializer_expr(self):
        return (self._default_expr and self._default_expr.initializer_expr
                or None)

    @property
    def initializer_deps(self):
        return (self._default_expr and self._default_expr.initializer_deps
                or None)

    @property
    def initializer_on_constructor(self):
        # In order to avoid cyclic header inclusion of IDL dictionaries, put
        # the initializer in *.cc if the type is a dictionary.
        if (self.initializer_expr and (self.initializer_deps  #
                                       or self.idl_type.unwrap().is_dictionary
                                       or self.idl_type.unwrap().is_union)):
            return self.initializer_expr
        return None

    @property
    def initializer_on_member_decl(self):
        # In order to avoid cyclic header inclusion of IDL dictionaries, put
        # the initializer in *.cc if the type is a dictionary.
        if (self.initializer_expr
                and not (self.initializer_deps
                         or self.idl_type.unwrap().is_dictionary
                         or self.idl_type.unwrap().is_union)):
            return self.initializer_expr
        idl_type = self.idl_type.unwrap(typedef=True)
        if idl_type.is_enumeration:
            # Since the IDL enumeration class is not default constructible,
            # construct the IDL enumeration with 0th enum value.  Note that
            # this is necessary only for compilation, and the value must never
            # be used due to the guard by `api_has` (`presence_expr`).
            return "static_cast<{}::Enum>(0)".format(self.type_info.value_t)
        return None

    @property
    def exposure(self):
        return self._exposure


def bind_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode

    local_vars = []

    local_vars.extend([
        S("class_like_name", ("const char* const ${class_like_name} = "
                              "\"${class_like.identifier}\";")),
        S("current_context", ("v8::Local<v8::Context> ${current_context} = "
                              "${isolate}->GetCurrentContext();")),
        S("is_cross_origin_isolated",
          ("const bool ${is_cross_origin_isolated} = "
           "${execution_context}->CrossOriginIsolatedCapability();")),
        S("is_in_isolated_context",
          ("const bool ${is_in_isolated_context} = "
           "${execution_context}->IsIsolatedContext();")),
        S("is_in_secure_context",
          ("const bool ${is_in_secure_context} = "
           "${execution_context}->IsSecureContext();")),
        S("v8_own_member_names", ("const auto& ${v8_own_member_names} = "
                                  "GetV8OwnMemberNames(${isolate});")),
    ])

    # execution_context
    node = S("execution_context",
             ("ExecutionContext* ${execution_context} = "
              "ExecutionContext::From(${current_context});"))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/execution_context/execution_context.h"
        ]))
    local_vars.append(node)

    code_node.register_code_symbols(local_vars)


def _constructor_needs_v8_isolate(dictionary):
    assert isinstance(dictionary, web_idl.Dictionary)

    return any(
        make_default_value_expr(member.idl_type,
                                member.default_value).initializer_deps
        for member in dictionary.members if member.default_value)


def make_factory_methods(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    decls = ListNode()
    defs = ListNode()

    dictionary = cg_context.dictionary

    if not _constructor_needs_v8_isolate(dictionary):
        func_def = CxxFuncDefNode(name="Create",
                                  arg_decls=[],
                                  return_type="${class_name}*",
                                  static=True)
        decls.append(func_def)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.append(
            TextNode("return MakeGarbageCollected<${class_name}>();"))

    func_def = CxxFuncDefNode(name="Create",
                              arg_decls=["v8::Isolate* isolate"],
                              return_type="${class_name}*",
                              static=True)
    decls.append(func_def)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.add_template_vars({"isolate": "isolate"})
    func_def.body.append(
        TextNode("return MakeGarbageCollected<${class_name}>(${isolate});"))

    func_decl = CxxFuncDeclNode(name="Create",
                                arg_decls=[
                                    "v8::Isolate* isolate",
                                    "v8::Local<v8::Value> v8_value",
                                    "ExceptionState& exception_state",
                                ],
                                return_type="${class_name}*",
                                static=True)
    decls.append(func_decl)
    func_def = CxxFuncDefNode(name="Create",
                              arg_decls=[
                                  "v8::Isolate* isolate",
                                  "v8::Local<v8::Value> v8_value",
                                  "ExceptionState& exception_state",
                              ],
                              return_type="${class_name}*",
                              class_name=cg_context.class_name)
    defs.append(func_def)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.add_template_vars({
        "isolate": "isolate",
        "v8_value": "v8_value",
        "exception_state": "exception_state",
    })
    bind_local_vars(body, cg_context)

    body.append(
        T("${class_name}* dictionary = "
          "MakeGarbageCollected<${class_name}>(${isolate});"))
    if not dictionary.has_required_member:
        body.append(
            CxxLikelyIfNode(cond="${v8_value}->IsNullOrUndefined()",
                            body=T("return dictionary;")))
    # [PermissiveDictionaryConversion]
    if "PermissiveDictionaryConversion" in dictionary.extended_attributes:
        body.append(
            CxxUnlikelyIfNode(cond="!${v8_value}->IsObject()",
                              body=[
                                  T("// [PermissiveDictionaryConversion]"),
                                  T("return dictionary;"),
                              ]))
    else:
        body.append(
            CxxUnlikelyIfNode(cond="!${v8_value}->IsObject()",
                              body=[
                                  T("${exception_state}.ThrowTypeError("
                                    "ExceptionMessages::ValueNotOfType("
                                    "${class_like_name}));"),
                                  T("return nullptr;"),
                              ]))
    body.extend([
        T("dictionary->FillMembersFromV8Object("
          "${isolate}, ${v8_value}.As<v8::Object>(), ${exception_state});"),
        CxxUnlikelyIfNode(cond="UNLIKELY(${exception_state}.HadException())",
                          body=T("return nullptr;")),
        T("return dictionary;"),
    ])

    return decls, defs


def make_constructors(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    decls = ListNode()
    defs = ListNode()

    member_initializer_list = [
        "{}({})".format(member.value_var, member.initializer_on_constructor)
        for member in cg_context.dictionary_own_members
        if member.initializer_on_constructor
    ]

    if not _constructor_needs_v8_isolate(cg_context.dictionary):
        func_decl = CxxFuncDeclNode(name=cg_context.class_name,
                                    arg_decls=[],
                                    return_type="",
                                    explicit=True)
        func_def = CxxFuncDefNode(
            name=cg_context.class_name,
            arg_decls=[],
            return_type="",
            class_name=cg_context.class_name,
            member_initializer_list=member_initializer_list)
        func_def.set_base_template_vars(cg_context.template_bindings())
        decls.append(func_decl)
        defs.append(func_def)
        defs.append(EmptyNode())

    if cg_context.dictionary.inherited:
        member_initializer_list = ["${base_class_name}(isolate)"
                                   ] + member_initializer_list
    func_decl = CxxFuncDeclNode(name=cg_context.class_name,
                                arg_decls=["v8::Isolate* isolate"],
                                return_type="",
                                explicit=True)
    func_def = CxxFuncDefNode(name=cg_context.class_name,
                              arg_decls=["v8::Isolate* isolate"],
                              return_type="",
                              class_name=cg_context.class_name,
                              member_initializer_list=member_initializer_list)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.add_template_vars({"isolate": "isolate"})
    decls.append(func_decl)
    defs.append(func_def)

    return decls, defs


def make_accessor_functions(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = FormatNode

    def make_check_assigned_value(member):
        idl_type = member.idl_type.unwrap(typedef=True)
        if idl_type.is_object:
            return F("DCHECK({}.IsObject());", member.value_var)
        if (member.type_info.is_gc_type and not idl_type.is_nullable):
            return F("DCHECK({});", member.value_var)
        return None

    def make_api_has(member):
        func_def = CxxFuncDefNode(name=member.api_has,
                                  arg_decls=[],
                                  return_type="bool",
                                  const=True)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.append(F("return {};", member.presence_expr))
        return func_def, None

    def make_api_get(member):
        func_def = CxxFuncDefNode(name=member.api_get,
                                  arg_decls=[],
                                  return_type=member.type_info.member_ref_t,
                                  const=True)
        func_def.set_base_template_vars(cg_context.template_bindings())
        if not member.is_always_present:
            func_def.body.append(F("DCHECK({}());", member.api_has))
        func_def.body.append(F("return {};", member.value_var))
        return func_def, None

    def make_api_get_or(member):
        func_def = CxxFuncDefNode(name=member.api_get_or,
                                  arg_decls=[
                                      "{} fallback_value".format(
                                          member.type_info.member_ref_t)
                                  ],
                                  return_type=member.type_info.value_t,
                                  const=True)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.extend([
            CxxUnlikelyIfNode(cond="!{}()".format(member.api_has),
                              body=T("return fallback_value;")),
            F("return {};", member.value_var),
        ])
        return func_def, None

    def make_api_get_or_copy_and_move(member):
        copy_func_decl = CxxFuncDeclNode(name=member.api_get_or,
                                         arg_decls=[
                                             "{} fallback_value".format(
                                                 member.type_info.member_ref_t)
                                         ],
                                         return_type=member.type_info.value_t,
                                         const=True)
        copy_func_def = CxxFuncDefNode(name=member.api_get_or,
                                       arg_decls=[
                                           "{} fallback_value".format(
                                               member.type_info.member_ref_t)
                                       ],
                                       return_type=member.type_info.value_t,
                                       class_name=cg_context.class_name,
                                       const=True)
        copy_func_def.set_base_template_vars(cg_context.template_bindings())
        copy_func_def.body.extend([
            CxxUnlikelyIfNode(cond="!{}()".format(member.api_has),
                              body=T("return fallback_value;")),
            F("return {};", member.value_var),
        ])

        move_func_decl = CxxFuncDeclNode(
            name=member.api_get_or,
            arg_decls=["{}&& fallback_value".format(member.type_info.value_t)],
            return_type=member.type_info.value_t,
            const=True)
        move_func_def = CxxFuncDefNode(
            name=member.api_get_or,
            arg_decls=["{}&& fallback_value".format(member.type_info.value_t)],
            return_type=member.type_info.value_t,
            class_name=cg_context.class_name,
            const=True)
        move_func_def.set_base_template_vars(cg_context.template_bindings())
        move_func_def.body.extend([
            CxxUnlikelyIfNode(cond="!{}()".format(member.api_has),
                              body=T("return std::move(fallback_value);")),
            F("return {};", member.value_var),
        ])

        decls = ListNode([copy_func_decl, move_func_decl])
        defs = ListNode([copy_func_def, EmptyNode(), move_func_def])
        return decls, defs

    def make_api_get_or_string(member):
        # getMemberOr(const char*) in addition to
        # getMemberOr(const String&) to avoid creation of a temporary String
        # object.
        if not member.idl_type.unwrap(typedef=True).is_string:
            return None, None
        func_def = CxxFuncDefNode(name=member.api_get_or,
                                  arg_decls=["const char* fallback_value"],
                                  return_type=member.type_info.value_t,
                                  const=True)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.extend([
            CxxUnlikelyIfNode(cond="!{}()".format(member.api_has),
                              body=T("return fallback_value;")),
            F("return {};", member.value_var),
        ])
        return func_def, None

    def make_api_set(member, type_info=None):
        if type_info is None:
            type_info = member.type_info
        func_def = CxxFuncDefNode(
            name=member.api_set,
            arg_decls=["{} value".format(type_info.member_ref_t)],
            return_type="void")
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.append(F("{} = value;", member.value_var))
        if member.does_use_presence_var:
            func_def.body.append(F("{} = true;", member.presence_var))
        func_def.body.append(make_check_assigned_value(member))
        return func_def, None

    def make_api_set_copy_and_move(member, type_info=None):
        if type_info is None:
            type_info = member.type_info
        copy_func_decl = CxxFuncDeclNode(
            name=member.api_set,
            arg_decls=["{} value".format(type_info.member_ref_t)],
            return_type="void")
        copy_func_def = CxxFuncDefNode(
            name=member.api_set,
            arg_decls=["{} value".format(type_info.member_ref_t)],
            return_type="void",
            class_name=cg_context.class_name)
        copy_func_def.set_base_template_vars(cg_context.template_bindings())
        copy_func_def.body.append(F("{} = value;", member.value_var))
        if member.does_use_presence_var:
            copy_func_def.body.append(F("{} = true;", member.presence_var))
        copy_func_def.body.append(make_check_assigned_value(member))

        move_func_decl = CxxFuncDeclNode(
            name=member.api_set,
            arg_decls=["{}&& value".format(type_info.value_t)],
            return_type="void")
        move_func_def = CxxFuncDefNode(
            name=member.api_set,
            arg_decls=["{}&& value".format(type_info.value_t)],
            return_type="void",
            class_name=cg_context.class_name)
        move_func_def.set_base_template_vars(cg_context.template_bindings())
        move_func_def.body.append(F("{} = std::move(value);",
                                    member.value_var))
        if member.does_use_presence_var:
            move_func_def.body.append(F("{} = true;", member.presence_var))
        move_func_def.body.append(make_check_assigned_value(member))

        decls = ListNode([copy_func_decl, move_func_decl])
        defs = ListNode([copy_func_def, EmptyNode(), move_func_def])
        return decls, defs

    def make_api_set_non_nullable(member):
        # setMember(InnerType) in addition to
        # setMember(absl::optional<InnerType>) for convenience.
        if not (member.idl_type.does_include_nullable_type
                and not member.type_info.has_null_value):
            return None, None
        return make_api_set(member, blink_type_info(member.idl_type.unwrap()))

    def make_api_set_copy_and_move_non_nullable(member):
        # setMember(InnerType) in addition to
        # setMember(absl::optional<InnerType>) for convenience.
        if not (member.idl_type.does_include_nullable_type
                and not member.type_info.has_null_value):
            return None, None
        return make_api_set_copy_and_move(
            member, blink_type_info(member.idl_type.unwrap()))

    def make_api_set_enum(member):
        # setMember(V8Enum::Enum) in addition to
        # setMember(V8Enum) for convenience.
        if not member.idl_type.unwrap().is_enumeration:
            return None, None
        type_info = blink_type_info(member.idl_type.unwrap())
        func_def = CxxFuncDefNode(
            name=member.api_set,
            arg_decls=["{}::Enum value".format(type_info.value_t)],
            return_type="void")
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.append(
            F("{} = {}(value);", member.value_var, type_info.value_t))
        if member.does_use_presence_var:
            func_def.body.append(F("{} = true;", member.presence_var))
        func_def.body.append(make_check_assigned_value(member))
        return func_def, None

    decls = ListNode()
    defs = ListNode()

    def add(func_decl, func_def):
        decls.append(func_decl)
        defs.append(func_def)
        defs.append(EmptyNode())

    for member in cg_context.dictionary_own_members:
        # Predicate
        add(*make_api_has(member))

        # Getter
        add(*make_api_get(member))
        if member.is_always_present:
            pass
        elif member.type_info.is_move_effective:
            add(*make_api_get_or_copy_and_move(member))
        else:
            add(*make_api_get_or(member))
            add(*make_api_get_or_string(member))

        # Setter
        if member.type_info.is_move_effective:
            add(*make_api_set_copy_and_move(member))
            add(*make_api_set_copy_and_move_non_nullable(member))
        else:
            add(*make_api_set(member))
            add(*make_api_set_non_nullable(member))
            add(*make_api_set_enum(member))

        decls.append(EmptyNode())

    return decls, defs


def make_backward_compatible_accessors(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    # TODO(crbug.com/1070871): Remove the accessors introduced just to be
    # backward compatible.

    F = FormatNode

    decls = ListNode()

    def make_api_has_non_null(member):
        func_def = CxxFuncDefNode(name=member.api_has_non_null,
                                  arg_decls=[],
                                  return_type="bool",
                                  const=True)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.append(
            F("return {}() && {}().has_value();", member.api_has,
              member.api_get))
        return func_def

    def make_api_get_non_null(member):
        func_def = CxxFuncDefNode(name=member.api_get_non_null,
                                  arg_decls=[],
                                  return_type=blink_type_info(
                                      member.idl_type.unwrap()).member_ref_t,
                                  const=True)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.extend([
            F("DCHECK({}());", member.api_has_non_null),
            F("return {}().value();", member.api_get),
        ])
        return func_def

    def make_api_set_enum_string(member):
        type_info = blink_type_info(member.idl_type.unwrap())
        func_def = CxxFuncDefNode(name=member.api_set,
                                  arg_decls=["const String& value"],
                                  return_type="void")
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.append(
            F("{} = {}::Create(value).value();", member.value_var,
              type_info.value_t))
        if member.does_use_presence_var:
            func_def.body.append(F("{} = true;", member.presence_var))
        return func_def

    for member in cg_context.dictionary_own_members:
        if member.idl_type.unwrap().is_enumeration:
            decls.append(make_api_set_enum_string(member))

        if (not member.idl_type.unwrap(nullable=False).is_nullable
                or member.type_info.has_null_value):
            continue
        # The Blink type is absl::optional<T>.
        decls.append(make_api_has_non_null(member))
        decls.append(make_api_get_non_null(member))

    if decls:
        decls.insert(0, TextNode("// Obsolete accessor functions"))

    return decls, None


def make_trace_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(name="Trace",
                                arg_decls=["Visitor* visitor"],
                                return_type="void",
                                const=True,
                                override=True)

    func_def = CxxFuncDefNode(name="Trace",
                              arg_decls=["Visitor* visitor"],
                              return_type="void",
                              class_name=cg_context.class_name,
                              const=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    for member in cg_context.dictionary_own_members:
        body.append(
            TextNode("TraceIfNeeded<{}>::Trace(visitor, {});".format(
                member.type_info.member_t, member.value_var)))
    body.append(TextNode("${base_class_name}::Trace(visitor);"))

    return func_decl, func_def


def make_blink_to_v8_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode
    T = TextNode
    F = FormatNode

    func_decl = CxxFuncDeclNode(name="FillV8ObjectWithMembers",
                                arg_decls=[
                                    "ScriptState* script_state",
                                    "v8::Local<v8::Object> v8_dictionary",
                                ],
                                return_type="bool",
                                const=True,
                                override=True)

    func_def = CxxFuncDefNode(name="FillV8ObjectWithMembers",
                              arg_decls=[
                                  "ScriptState* script_state",
                                  "v8::Local<v8::Object> v8_dictionary",
                              ],
                              return_type="bool",
                              class_name=cg_context.class_name,
                              const=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.add_template_vars({
        "script_state": "script_state",
        "v8_dictionary": "v8_dictionary",
    })
    body.register_code_symbols([
        S("isolate",
          "v8::Isolate* ${isolate} = ${script_state}->GetIsolate();"),
        S("v8_value", "v8::Local<v8::Value> ${v8_value};"),
        S("was_property_created", "bool ${was_property_created};"),
    ])
    bind_local_vars(body, cg_context)

    if cg_context.dictionary.inherited:
        body.extend([
            CxxUnlikelyIfNode(  #
                cond=T("!${base_class_name}::FillV8ObjectWithMembers"
                       "(${script_state}, ${v8_dictionary})"),
                body=T("return false;")),
            EmptyNode(),
        ])

    for index, member in enumerate(cg_context.dictionary_own_members):
        node = CxxLikelyIfNode(
            cond="{}()".format(member.api_has),
            body=[
                CxxUnlikelyIfNode(  #
                    cond=_format(
                        "!ToV8Traits<{native_value_tag}>::"
                        "ToV8(${script_state}, {blink_value})"
                        ".ToLocal(&${v8_value})",
                        native_value_tag=native_value_tag(member.idl_type),
                        blink_value=member.value_var),
                    body=T("return false;")),
                CxxUnlikelyIfNode(  #
                    cond=_format(
                        "!${v8_dictionary}->CreateDataProperty("
                        "${current_context}, "
                        "${v8_own_member_names}[{index}].Get(${isolate}), "
                        "${v8_value}).To(&${was_property_created})",
                        index=index),
                    body=T("return false;")),
            ])

        conditional = expr_from_exposure(member.exposure)
        if not conditional.is_always_true:
            node = CxxLikelyIfNode(cond=conditional, body=node)

        body.append(node)

    body.append(TextNode("return true;"))

    return func_decl, func_def


def make_v8_to_blink_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode
    T = TextNode
    F = FormatNode

    func_decl = CxxFuncDeclNode(name="FillMembersFromV8Object",
                                arg_decls=[
                                    "v8::Isolate* isolate",
                                    "v8::Local<v8::Object> v8_dictionary",
                                    "ExceptionState& exception_state",
                                ],
                                return_type="void")

    func_def = CxxFuncDefNode(name="FillMembersFromV8Object",
                              arg_decls=[
                                  "v8::Isolate* isolate",
                                  "v8::Local<v8::Object> v8_dictionary",
                                  "ExceptionState& exception_state",
                              ],
                              return_type="void",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.add_template_vars({
        "isolate": "isolate",
        "v8_dictionary": "v8_dictionary",
        "exception_state": "exception_state",
    })
    body.register_code_symbols([
        S("exception_context_scope",
          ("ExceptionState::ContextScope ${exception_context_scope}("
           "ExceptionContext("
           "ExceptionContext::Context::kDictionaryMemberGet, "
           "${class_like_name}, \"\"), "
           "${exception_state});")),
        S("fallback_presence_var", "bool ${fallback_presence_var};"),
        S("is_optional", "constexpr bool ${is_optional} = false;"),
        S("is_required", "constexpr bool ${is_required} = true;"),
        S("try_block", "v8::TryCatch ${try_block}(${isolate});"),
    ])
    bind_local_vars(body, cg_context)

    if cg_context.dictionary.inherited:
        body.extend([
            T("${base_class_name}::FillMembersFromV8Object"
              "(${isolate}, ${v8_dictionary}, ${exception_state});"),
            CxxUnlikelyIfNode(
                cond="UNLIKELY(${exception_state}.HadException())",
                body=T("return;")),
            EmptyNode(),
        ])

    for index, member in enumerate(cg_context.dictionary_own_members):
        cond = _format(
            "!bindings::GetDictionaryMemberFromV8Object"
            "<{native_value_tag}, {is_required}>("
            "${isolate}, ${current_context}, "
            "${v8_dictionary}, "
            "${v8_own_member_names}[{index}].Get(${isolate}), "
            "{presence_var}, {value_var}, "
            "${try_block}, ${exception_state})",
            native_value_tag=native_value_tag(member.idl_type),
            is_required=("${is_required}"
                         if member.is_required else "${is_optional}"),
            index=index,
            presence_var=(member.presence_var if member.does_use_presence_var
                          else "${fallback_presence_var}"),
            value_var=member.value_var)
        node = SequenceNode([
            F(("${exception_context_scope}"
               ".ChangePropertyNameAsOptimizationHack(\"{member_name}\");"),
              member_name=member.identifier),
            CxxUnlikelyIfNode(cond=cond, body=T("return;")),
        ])

        conditional = expr_from_exposure(member.exposure)
        if not conditional.is_always_true:
            node = CxxLikelyIfNode(cond=conditional, body=node)

        body.append(node)

    return func_decl, func_def


def make_v8_own_member_names_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(
        name="GetV8OwnMemberNames",
        arg_decls=["v8::Isolate* isolate"],
        return_type="const base::span<const v8::Eternal<v8::Name>>",
        static=True)

    func_def = CxxFuncDefNode(
        name="GetV8OwnMemberNames",
        arg_decls=["v8::Isolate* isolate"],
        return_type="const base::span<const v8::Eternal<v8::Name>>",
        class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    func_def.body.add_template_vars({"isolate": "isolate"})

    if not cg_context.dictionary.own_members:
        body.append(TextNode("return {};"))
        return func_decl, func_def

    body.extend([
        ListNode([
            TextNode("static const char* const kOwnMemberNames[] = {"),
            ListNode([
                TextNode("\"{}\",".format(member.identifier))
                for member in cg_context.dictionary.own_members
            ]),
            TextNode("};"),
        ]),
        TextNode("return V8PerIsolateData::From(${isolate})"
                 "->FindOrCreateEternalNameCache"
                 "(kOwnMemberNames, kOwnMemberNames);"),
    ])

    return func_decl, func_def


def make_member_vars_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    F = FormatNode

    presence_vars = ListNode([
        F("bool {} = false;", member.presence_var)
        for member in cg_context.dictionary_own_members
        if member.does_use_presence_var
    ])

    value_vars = ListNode([
        F("{} {}{};", member.type_info.member_t, member.value_var,
          ("{{{}}}".format(member.initializer_on_member_decl)
           if member.initializer_on_member_decl else ""))
        for member in cg_context.dictionary_own_members
    ])

    return ListNode([
        presence_vars,
        EmptyNode(),
        value_vars,
    ])


def generate_dictionary(dictionary_identifier):
    assert isinstance(dictionary_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    dictionary = web_idl_database.find(dictionary_identifier)

    path_manager = PathManager(dictionary)
    assert path_manager.api_component == path_manager.impl_component, (
        "Partial dictionaries across components are not supported.")
    api_component = path_manager.api_component
    for_testing = dictionary.code_generator_info.for_testing

    # Class names
    class_name = blink_class_name(dictionary)
    if dictionary.inherited:
        base_class_name = blink_class_name(dictionary.inherited)
    else:
        base_class_name = "bindings::DictionaryBase"

    cg_context = CodeGenContext(dictionary=dictionary,
                                dictionary_own_members=tuple(
                                    map(_DictionaryMember,
                                        dictionary.own_members)),
                                class_name=class_name,
                                base_class_name=base_class_name)

    # Filepaths
    header_path = path_manager.api_path(ext="h")
    source_path = path_manager.api_path(ext="cc")

    # Root nodes
    header_node = ListNode(tail="\n")
    header_node.set_accumulator(CodeGenAccumulator())
    header_node.set_renderer(MakoRenderer())
    source_node = ListNode(tail="\n")
    source_node.set_accumulator(CodeGenAccumulator())
    source_node.set_renderer(MakoRenderer())

    # Namespaces
    header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))

    # Class definition
    class_def = CxxClassDefNode(cg_context.class_name,
                                base_class_names=[cg_context.base_class_name],
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    # Implementation parts
    factory_decls, factory_defs = make_factory_methods(cg_context)
    ctor_decls, ctor_defs = make_constructors(cg_context)
    accessor_decls, accessor_defs = make_accessor_functions(cg_context)
    backward_compatible_accessor_decls, backward_compatible_accessor_defs = (
        make_backward_compatible_accessors(cg_context))
    trace_func_decls, trace_func_defs = make_trace_function(cg_context)
    blink_to_v8_decls, blink_to_v8_defs = make_blink_to_v8_function(cg_context)
    v8_to_blink_decls, v8_to_blink_defs = make_v8_to_blink_function(cg_context)
    v8_names_decls, v8_names_defs = make_v8_own_member_names_function(
        cg_context)
    member_vars_def = make_member_vars_def(cg_context)

    # Header part (copyright, include directives, and forward declarations)
    header_node.extend([
        make_copyright_header(),
        EmptyNode(),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(header_node.accumulator),
                EmptyNode(),
                header_blink_ns,
            ]), name_style.header_guard(header_path)),
    ])
    header_blink_ns.body.extend([
        make_forward_declarations(header_node.accumulator),
        EmptyNode(),
    ])
    source_node.extend([
        make_copyright_header(),
        EmptyNode(),
        TextNode("#include \"{}\"".format(header_path)),
        EmptyNode(),
        make_header_include_directives(source_node.accumulator),
        EmptyNode(),
        source_blink_ns,
    ])
    source_blink_ns.body.extend([
        make_forward_declarations(source_node.accumulator),
        EmptyNode(),
    ])

    # Assemble the parts.
    header_node.accumulator.add_class_decls(["ExceptionState"])
    header_node.accumulator.add_include_headers([
        (PathManager(dictionary.inherited).api_path(
            ext="h") if dictionary.inherited else
         "third_party/blink/renderer/platform/bindings/dictionary_base.h"),
        "base/containers/span.h",
        component_export_header(api_component, for_testing),
    ])
    source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
        "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h",
        "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h",
        "third_party/blink/renderer/platform/bindings/exception_messages.h",
        "third_party/blink/renderer/platform/bindings/exception_state.h",
        "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h",
    ])
    (header_forward_decls, header_include_headers, source_forward_decls,
     source_include_headers) = collect_forward_decls_and_include_headers(
         list(map(lambda member: member.idl_type, dictionary.own_members)))
    header_node.accumulator.add_class_decls(header_forward_decls)
    header_node.accumulator.add_include_headers(header_include_headers)
    source_node.accumulator.add_class_decls(source_forward_decls)
    source_node.accumulator.add_include_headers(source_include_headers)

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(factory_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(factory_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(ctor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(ctor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(accessor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(accessor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(backward_compatible_accessor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(backward_compatible_accessor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(trace_func_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(trace_func_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.protected_section.append(blink_to_v8_decls)
    class_def.protected_section.append(EmptyNode())
    source_blink_ns.body.append(blink_to_v8_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.protected_section.append(v8_to_blink_decls)
    class_def.protected_section.append(EmptyNode())
    source_blink_ns.body.append(v8_to_blink_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.private_section.append(v8_names_decls)
    class_def.private_section.append(EmptyNode())
    source_blink_ns.body.append(v8_names_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.private_section.append(member_vars_def)
    class_def.private_section.append(EmptyNode())

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_dictionaries(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for dictionary in web_idl_database.dictionaries:
        task_queue.post_task(generate_dictionary, dictionary.identifier)
