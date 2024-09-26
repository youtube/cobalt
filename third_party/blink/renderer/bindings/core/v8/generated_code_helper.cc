// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_declaration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/xml/dom_parser.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

enum class IgnorePause { kDontIgnore, kIgnore };

// 'beforeunload' event listeners are runnable even when execution contexts are
// paused. Use |RespectPause::kPrioritizeOverPause| in such a case.
bool IsCallbackFunctionRunnableInternal(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state,
    IgnorePause ignore_pause) {
  if (!callback_relevant_script_state->ContextIsValid())
    return false;
  const ExecutionContext* relevant_execution_context =
      ExecutionContext::From(callback_relevant_script_state);
  if (!relevant_execution_context ||
      relevant_execution_context->IsContextDestroyed()) {
    return false;
  }
  if (relevant_execution_context->IsContextPaused()) {
    if (ignore_pause == IgnorePause::kDontIgnore)
      return false;
  }

  // TODO(yukishiino): Callback function type value must make the incumbent
  // environment alive, i.e. the reference to v8::Context must be strong.
  v8::HandleScope handle_scope(incumbent_script_state->GetIsolate());
  v8::Local<v8::Context> incumbent_context =
      incumbent_script_state->GetContext();
  ExecutionContext* incumbent_execution_context =
      incumbent_context.IsEmpty() ? nullptr
                                  : ToExecutionContext(incumbent_context);
  // The incumbent realm schedules the currently-running callback although it
  // may not correspond to the currently-running function object. So we check
  // the incumbent context which originally schedules the currently-running
  // callback to see whether the script setting is disabled before invoking
  // the callback.
  // TODO(crbug.com/608641): move IsMainWorld check into
  // ExecutionContext::CanExecuteScripts()
  if (!incumbent_execution_context ||
      incumbent_execution_context->IsContextDestroyed()) {
    return false;
  }
  if (incumbent_execution_context->IsContextPaused()) {
    if (ignore_pause == IgnorePause::kDontIgnore)
      return false;
  }
  return !incumbent_script_state->World().IsMainWorld() ||
         incumbent_execution_context->CanExecuteScripts(kAboutToExecuteScript);
}

}  // namespace

bool IsCallbackFunctionRunnable(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state) {
  return IsCallbackFunctionRunnableInternal(callback_relevant_script_state,
                                            incumbent_script_state,
                                            IgnorePause::kDontIgnore);
}

bool IsCallbackFunctionRunnableIgnoringPause(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state) {
  return IsCallbackFunctionRunnableInternal(callback_relevant_script_state,
                                            incumbent_script_state,
                                            IgnorePause::kIgnore);
}

void ExceptionToRejectPromiseScope::ConvertExceptionToRejectPromise() {
  // As exceptions must always be created in the current realm, reject
  // promises must also be created in the current realm while regular promises
  // are created in the relevant realm of the context object.
  ScriptState* script_state = ScriptState::ForCurrentRealm(info_);
  V8SetReturnValue(
      info_, ScriptPromise::Reject(script_state, exception_state_).V8Value());
}

namespace bindings {

void SetupIDLInterfaceTemplate(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::FunctionTemplate> parent_interface_template) {
  v8::Local<v8::String> class_string =
      V8AtomicString(isolate, wrapper_type_info->interface_name);

  if (!parent_interface_template.IsEmpty())
    interface_template->Inherit(parent_interface_template);
  interface_template->ReadOnlyPrototype();
  interface_template->SetClassName(class_string);

  prototype_template->Set(
      v8::Symbol::GetToStringTag(isolate), class_string,
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));

  instance_template->SetInternalFieldCount(kV8DefaultWrapperInternalFieldCount);
}

void SetupIDLNamespaceTemplate(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::ObjectTemplate> interface_template) {
  v8::Local<v8::String> class_string =
      V8AtomicString(isolate, wrapper_type_info->interface_name);

  interface_template->Set(
      v8::Symbol::GetToStringTag(isolate), class_string,
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));
}

void SetupIDLCallbackInterfaceTemplate(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::FunctionTemplate> interface_template) {
  interface_template->RemovePrototype();
  interface_template->SetClassName(
      V8AtomicString(isolate, wrapper_type_info->interface_name));
}

void SetupIDLObservableArrayBackingListTemplate(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::FunctionTemplate> interface_template) {
  interface_template->SetClassName(
      V8AtomicString(isolate, wrapper_type_info->interface_name));

  instance_template->SetInternalFieldCount(kV8DefaultWrapperInternalFieldCount);
}

void SetupIDLSyncIteratorTemplate(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Intrinsic parent_intrinsic_prototype,
    const char* class_string) {
  DCHECK(parent_intrinsic_prototype == v8::Intrinsic::kIteratorPrototype ||
         parent_intrinsic_prototype == v8::Intrinsic::kMapIteratorPrototype ||
         parent_intrinsic_prototype == v8::Intrinsic::kSetIteratorPrototype);

  v8::Local<v8::String> v8_class_string = V8String(isolate, class_string);

  // https://webidl.spec.whatwg.org/#es-iterator-prototype-object
  // https://webidl.spec.whatwg.org/#es-map-iterator
  // https://webidl.spec.whatwg.org/#es-set-iterator
  v8::Local<v8::FunctionTemplate>
      intrinsic_iterator_prototype_interface_template =
          v8::FunctionTemplate::New(isolate, nullptr, v8::Local<v8::Value>(),
                                    v8::Local<v8::Signature>(), 0,
                                    v8::ConstructorBehavior::kThrow);
  // It's not clear whether we need to remove the existing prototype object
  // before we replace it with another object. Despite that the following test
  // in V8 removes the existing one before setting a new one with a comment,
  // it's not yet crystal clear if RemovePrototype() is mandatory or not.
  // https://source.chromium.org/chromium/chromium/src/+/main:v8/test/cctest/test-api.cc;l=25249;drc=00a341994fa5cc0b41ffa0e886eeef67fce0c804
  intrinsic_iterator_prototype_interface_template->RemovePrototype();
  intrinsic_iterator_prototype_interface_template->SetIntrinsicDataProperty(
      V8AtomicString(isolate, "prototype"), parent_intrinsic_prototype);
  interface_template->Inherit(intrinsic_iterator_prototype_interface_template);

  interface_template->ReadOnlyPrototype();
  interface_template->SetClassName(v8_class_string);

  prototype_template->Set(
      v8::Symbol::GetToStringTag(isolate), v8_class_string,
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));

  instance_template->SetInternalFieldCount(kV8DefaultWrapperInternalFieldCount);
}

absl::optional<size_t> FindIndexInEnumStringTable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    base::span<const char* const> enum_value_table,
    const char* enum_type_name,
    ExceptionState& exception_state) {
  const String& str_value = NativeValueTraits<IDLString>::NativeValue(
      isolate, value, exception_state);
  if (UNLIKELY(exception_state.HadException()))
    return absl::nullopt;

  absl::optional<size_t> index =
      FindIndexInEnumStringTable(str_value, enum_value_table);

  if (UNLIKELY(!index.has_value())) {
    exception_state.ThrowTypeError("The provided value '" + str_value +
                                   "' is not a valid enum value of type " +
                                   enum_type_name + ".");
  }
  return index;
}

absl::optional<size_t> FindIndexInEnumStringTable(
    const String& str_value,
    base::span<const char* const> enum_value_table) {
  for (size_t i = 0; i < enum_value_table.size(); ++i) {
    if (Equal(str_value.Impl(), enum_value_table[i]))
      return i;
  }
  return absl::nullopt;
}

void ReportInvalidEnumSetToAttribute(v8::Isolate* isolate,
                                     const String& value,
                                     const String& enum_type_name,
                                     ExceptionState& exception_state) {
  ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  exception_state.ThrowTypeError("The provided value '" + value +
                                 "' is not a valid enum value of type " +
                                 enum_type_name + ".");
  String message = exception_state.Message();
  exception_state.ClearException();

  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message,
      CaptureSourceLocation(execution_context)));
}

bool IsEsIterableObject(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
  // https://webidl.spec.whatwg.org/#es-overloads
  // step 9. Otherwise: if Type(V) is Object and ...
  if (!value->IsObject())
    return false;

  // step 9.1. Let method be ? GetMethod(V, @@iterator).
  // https://tc39.es/ecma262/#sec-getmethod
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> iterator_key = v8::Symbol::GetIterator(isolate);
  v8::Local<v8::Value> iterator_value;
  if (!value.As<v8::Object>()
           ->Get(isolate->GetCurrentContext(), iterator_key)
           .ToLocal(&iterator_value)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }

  if (iterator_value->IsNullOrUndefined())
    return false;

  if (!iterator_value->IsFunction()) {
    exception_state.ThrowTypeError("@@iterator must be a function");
    return false;
  }

  return true;
}

Document* ToDocumentFromExecutionContext(ExecutionContext* execution_context) {
  return DynamicTo<LocalDOMWindow>(execution_context)->document();
}

ExecutionContext* ExecutionContextFromV8Wrappable(const Range* range) {
  return range->startContainer()->GetExecutionContext();
}

ExecutionContext* ExecutionContextFromV8Wrappable(const DOMParser* parser) {
  return parser->GetWindow();
}

v8::MaybeLocal<v8::Value> CreateNamedConstructorFunction(
    ScriptState* script_state,
    v8::FunctionCallback callback,
    const char* func_name,
    int func_length,
    const WrapperTypeInfo* wrapper_type_info) {
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  const void* callback_key = reinterpret_cast<const void*>(callback);

  if (!script_state->ContextIsValid()) {
    return v8::Undefined(isolate);
  }

  v8::Local<v8::FunctionTemplate> function_template =
      per_isolate_data->FindV8Template(world, callback_key)
          .As<v8::FunctionTemplate>();
  if (function_template.IsEmpty()) {
    function_template = v8::FunctionTemplate::New(
        isolate, callback, v8::Local<v8::Value>(), v8::Local<v8::Signature>(),
        func_length, v8::ConstructorBehavior::kAllow,
        v8::SideEffectType::kHasSideEffect);
    v8::Local<v8::FunctionTemplate> interface_template =
        wrapper_type_info->GetV8ClassTemplate(isolate, world)
            .As<v8::FunctionTemplate>();
    function_template->Inherit(interface_template);
    function_template->SetClassName(V8AtomicString(isolate, func_name));
    function_template->InstanceTemplate()->SetInternalFieldCount(
        kV8DefaultWrapperInternalFieldCount);
    per_isolate_data->AddV8Template(world, callback_key, function_template);
  }

  v8::Local<v8::Context> context = script_state->GetContext();
  V8PerContextData* per_context_data = V8PerContextData::From(context);
  v8::Local<v8::Function> function;
  if (!function_template->GetFunction(context).ToLocal(&function)) {
    return v8::MaybeLocal<v8::Value>();
  }
  v8::Local<v8::Object> prototype_object =
      per_context_data->PrototypeForType(wrapper_type_info);
  bool did_define;
  if (!function
           ->DefineOwnProperty(
               context, V8AtomicString(isolate, "prototype"), prototype_object,
               static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum |
                                                  v8::DontDelete))
           .To(&did_define)) {
    return v8::MaybeLocal<v8::Value>();
  }
  CHECK(did_define);
  return function;
}

void InstallUnscopablePropertyNames(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> prototype_object,
    base::span<const char* const> property_name_table) {
  // 3.6.3. Interface prototype object
  // https://webidl.spec.whatwg.org/#interface-prototype-object
  // step 8. If interface has any member declared with the [Unscopable]
  //   extended attribute, then:
  // step 8.1. Let unscopableObject be the result of performing
  //   ! ObjectCreate(null).
  v8::Local<v8::Object> unscopable_object =
      v8::Object::New(isolate, v8::Null(isolate), nullptr, nullptr, 0);
  for (const char* const property_name : property_name_table) {
    // step 8.2.2. Perform ! CreateDataProperty(unscopableObject, id, true).
    unscopable_object
        ->CreateDataProperty(context, V8AtomicString(isolate, property_name),
                             v8::True(isolate))
        .ToChecked();
  }
  // step 8.3. Let desc be the PropertyDescriptor{[[Value]]: unscopableObject,
  //   [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true}.
  // step 8.4. Perform ! DefinePropertyOrThrow(interfaceProtoObj,
  //   @@unscopables, desc).
  prototype_object
      ->DefineOwnProperty(
          context, v8::Symbol::GetUnscopables(isolate), unscopable_object,
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum))
      .ToChecked();
}

v8::Local<v8::Array> EnumerateIndexedProperties(v8::Isolate* isolate,
                                                uint32_t length) {
  Vector<v8::Local<v8::Value>> elements;
  elements.reserve(length);
  for (uint32_t i = 0; i < length; ++i)
    elements.UncheckedAppend(v8::Integer::New(isolate, i));
  return v8::Array::New(isolate, elements.data(), elements.size());
}

void InstallCSSPropertyAttributes(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const char* const> css_property_names) {
  const String kGetPrefix = "get ";
  const String kSetPrefix = "set ";
  for (const char* const property_name : css_property_names) {
    v8::Local<v8::Value> v8_property_name = v8::External::New(
        isolate,
        const_cast<void*>(reinterpret_cast<const void*>(property_name)));
    v8::Local<v8::FunctionTemplate> get_func = v8::FunctionTemplate::New(
        isolate, CSSPropertyAttributeGet, v8_property_name, signature, 0,
        v8::ConstructorBehavior::kThrow, v8::SideEffectType::kHasNoSideEffect);
    v8::Local<v8::FunctionTemplate> set_func = v8::FunctionTemplate::New(
        isolate, CSSPropertyAttributeSet, v8_property_name, signature, 1,
        v8::ConstructorBehavior::kThrow, v8::SideEffectType::kHasSideEffect);
    get_func->SetAcceptAnyReceiver(false);
    set_func->SetAcceptAnyReceiver(false);
    get_func->SetClassName(
        V8AtomicString(isolate, String(kGetPrefix + property_name)));
    set_func->SetClassName(
        V8AtomicString(isolate, String(kSetPrefix + property_name)));
    prototype_template->SetAccessorProperty(
        V8AtomicString(isolate, property_name), get_func, set_func);
  }
}

void CSSPropertyAttributeGet(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CSSStyleDeclaration* blink_receiver =
      V8CSSStyleDeclaration::ToWrappableUnsafe(info.This());
  const char* property_name =
      reinterpret_cast<const char*>(info.Data().As<v8::External>()->Value());
  // TODO(andruud): AnonymousNamedGetter is not the best function.  Change the
  // function to a more appropriate one.
  auto&& return_value = blink_receiver->AnonymousNamedGetter(property_name);
  bindings::V8SetReturnValue(info, return_value, info.GetIsolate(),
                             bindings::V8ReturnValue::kNonNullable);
}

void CSSPropertyAttributeSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  const char* const class_like_name = "CSSStyleDeclaration";
  const char* property_name =
      reinterpret_cast<const char*>(info.Data().As<v8::External>()->Value());
  ExceptionState exception_state(isolate, ExceptionState::kSetterContext,
                                 class_like_name, property_name);

  // [CEReactions]
  CEReactionsScope ce_reactions_scope;

  v8::Local<v8::Object> v8_receiver = info.This();
  CSSStyleDeclaration* blink_receiver =
      V8CSSStyleDeclaration::ToWrappableUnsafe(v8_receiver);
  v8::Local<v8::Value> v8_property_value = info[0];
  auto&& arg1_value = NativeValueTraits<IDLAny>::NativeValue(
      isolate, v8_property_value, exception_state);
  if (UNLIKELY(exception_state.HadException())) {
    return;
  }
  v8::Local<v8::Context> receiver_context =
      v8_receiver->GetCreationContextChecked();
  ScriptState* receiver_script_state = ScriptState::From(receiver_context);
  // TODO(andruud): AnonymousNamedSetter is not the best function.  Change the
  // function to a more appropriate one.  It's better to pass |exception_state|
  // as the implementation of AnonymousNamedSetter needs it.
  blink_receiver->AnonymousNamedSetter(receiver_script_state, property_name,
                                       arg1_value);
}

template <typename IDLType,
          typename ArgType,
          void (Element::*MemFunc)(const QualifiedName&, ArgType)>
void PerformAttributeSetCEReactionsReflect(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name) {
  v8::Isolate* isolate = info.GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kSetterContext,
                                 interface_name, attribute_name);
  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  CEReactionsScope ce_reactions_scope;

  Element* blink_receiver = V8Element::ToWrappableUnsafe(info.This());
  auto&& arg_value = NativeValueTraits<IDLType>::NativeValue(isolate, info[0],
                                                             exception_state);
  if (UNLIKELY(exception_state.HadException()))
    return;

  (blink_receiver->*MemFunc)(content_attribute, arg_value);
}

void PerformAttributeSetCEReactionsReflectTypeBoolean(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name) {
  PerformAttributeSetCEReactionsReflect<IDLBoolean, bool,
                                        &Element::SetBooleanAttribute>(
      info, content_attribute, interface_name, attribute_name);
}

void PerformAttributeSetCEReactionsReflectTypeString(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name) {
  PerformAttributeSetCEReactionsReflect<IDLString, const AtomicString&,
                                        &Element::setAttribute>(
      info, content_attribute, interface_name, attribute_name);
}

void PerformAttributeSetCEReactionsReflectTypeStringLegacyNullToEmptyString(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name) {
  PerformAttributeSetCEReactionsReflect<IDLStringTreatNullAsEmptyString,
                                        const AtomicString&,
                                        &Element::setAttribute>(
      info, content_attribute, interface_name, attribute_name);
}

void PerformAttributeSetCEReactionsReflectTypeStringOrNull(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name) {
  PerformAttributeSetCEReactionsReflect<
      IDLNullable<IDLString>, const AtomicString&, &Element::setAttribute>(
      info, content_attribute, interface_name, attribute_name);
}

}  // namespace bindings

}  // namespace blink
