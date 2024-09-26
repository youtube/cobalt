// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"

#include <memory>

#include "base/check.h"
#include "base/rand_util.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_token_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_all_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

ThreadDebuggerCommonImpl::ThreadDebuggerCommonImpl(v8::Isolate* isolate)
    : ThreadDebugger(isolate), isolate_(isolate) {}

ThreadDebuggerCommonImpl::~ThreadDebuggerCommonImpl() = default;

// static
mojom::ConsoleMessageLevel
ThreadDebuggerCommonImpl::V8MessageLevelToMessageLevel(
    v8::Isolate::MessageErrorLevel level) {
  mojom::ConsoleMessageLevel result = mojom::ConsoleMessageLevel::kInfo;
  switch (level) {
    case v8::Isolate::kMessageDebug:
      result = mojom::ConsoleMessageLevel::kVerbose;
      break;
    case v8::Isolate::kMessageWarning:
      result = mojom::ConsoleMessageLevel::kWarning;
      break;
    case v8::Isolate::kMessageError:
      result = mojom::ConsoleMessageLevel::kError;
      break;
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
    default:
      result = mojom::ConsoleMessageLevel::kInfo;
      break;
  }
  return result;
}

void ThreadDebuggerCommonImpl::AsyncTaskScheduled(
    const StringView& operation_name,
    void* task,
    bool recurring) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskScheduled(ToV8InspectorStringView(operation_name),
                                    task, recurring);
}

void ThreadDebuggerCommonImpl::AsyncTaskCanceled(void* task) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskCanceled(task);
}

void ThreadDebuggerCommonImpl::AllAsyncTasksCanceled() {
  v8_inspector_->allAsyncTasksCanceled();
}

void ThreadDebuggerCommonImpl::AsyncTaskStarted(void* task) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskStarted(task);
}

void ThreadDebuggerCommonImpl::AsyncTaskFinished(void* task) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskFinished(task);
}

v8_inspector::V8StackTraceId ThreadDebuggerCommonImpl::StoreCurrentStackTrace(
    const StringView& description) {
  return v8_inspector_->storeCurrentStackTrace(
      ToV8InspectorStringView(description));
}

void ThreadDebuggerCommonImpl::ExternalAsyncTaskStarted(
    const v8_inspector::V8StackTraceId& parent) {
  v8_inspector_->externalAsyncTaskStarted(parent);
}

void ThreadDebuggerCommonImpl::ExternalAsyncTaskFinished(
    const v8_inspector::V8StackTraceId& parent) {
  v8_inspector_->externalAsyncTaskFinished(parent);
}

unsigned ThreadDebuggerCommonImpl::PromiseRejected(
    v8::Local<v8::Context> context,
    const String& error_message,
    v8::Local<v8::Value> exception,
    std::unique_ptr<SourceLocation> location) {
  const String default_message = "Uncaught (in promise)";
  String message = error_message;
  if (message.empty())
    message = default_message;
  else if (message.StartsWith("Uncaught "))
    message = message.Substring(0, 8) + " (in promise)" + message.Substring(8);

  ReportConsoleMessage(
      ToExecutionContext(context), mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, message, location.get());
  String url = location->Url();
  return GetV8Inspector()->exceptionThrown(
      context, ToV8InspectorStringView(default_message), exception,
      ToV8InspectorStringView(message), ToV8InspectorStringView(url),
      location->LineNumber(), location->ColumnNumber(),
      location->TakeStackTrace(), location->ScriptId());
}

void ThreadDebuggerCommonImpl::PromiseRejectionRevoked(
    v8::Local<v8::Context> context,
    unsigned promise_rejection_id) {
  const String message = "Handler added to rejected promise";
  GetV8Inspector()->exceptionRevoked(context, promise_rejection_id,
                                     ToV8InspectorStringView(message));
}

// TODO(mustaq): Is it tied to a specific user action? https://crbug.com/826293
void ThreadDebuggerCommonImpl::beginUserGesture() {
  auto* window = CurrentDOMWindow(isolate_);
  LocalFrame::NotifyUserActivation(
      window ? window->GetFrame() : nullptr,
      mojom::blink::UserActivationNotificationType::kDevTools);
}

namespace {
static const char kType[] = "type";
static const char kValue[] = "value";

v8::Local<v8::String> TypeStringKey(v8::Isolate* isolate_) {
  return V8String(isolate_, kType);
}
v8::Local<v8::String> ValueStringKey(v8::Isolate* isolate_) {
  return V8String(isolate_, kValue);
}

v8::Local<v8::Object> SerializeNodeToV8Object(Node* node,
                                              v8::Isolate* isolate_,
                                              int max_depth) {
  static const char kAttributes[] = "attributes";
  static const char kBackendNodeId[] = "backendNodeId";
  static const char kChildren[] = "children";
  static const char kChildNodeCount[] = "childNodeCount";
  static const char kLocalName[] = "localName";
  static const char kNamespaceURI[] = "namespaceURI";
  static const char kNode[] = "node";
  static const char kNodeType[] = "nodeType";
  static const char kNodeValue[] = "nodeValue";

  Vector<v8::Local<v8::Name>> serialized_value_keys;
  Vector<v8::Local<v8::Value>> serialized_value_values;
  serialized_value_keys.push_back(V8String(isolate_, kNodeType));
  serialized_value_values.push_back(
      v8::Number::New(isolate_, node->getNodeType()));

  if (!node->nodeValue().IsNull()) {
    serialized_value_keys.push_back(V8String(isolate_, kNodeValue));
    serialized_value_values.push_back(V8String(isolate_, node->nodeValue()));
  }

  DOMNodeId backend_node_id = DOMNodeIds::IdForNode(node);
  serialized_value_keys.push_back(V8String(isolate_, kBackendNodeId));
  serialized_value_values.push_back(v8::Number::New(isolate_, backend_node_id));

  if (node->IsElementNode()) {
    Element* element = To<Element>(node);

    serialized_value_keys.push_back(V8String(isolate_, kLocalName));
    serialized_value_values.push_back(V8String(isolate_, element->localName()));

    serialized_value_keys.push_back(V8String(isolate_, kNamespaceURI));
    serialized_value_values.push_back(
        V8String(isolate_, element->namespaceURI()));

    serialized_value_keys.push_back(V8String(isolate_, kChildNodeCount));
    serialized_value_values.push_back(
        v8::Number::New(isolate_, node->CountChildren()));

    Vector<v8::Local<v8::Name>> node_attributes_keys;
    Vector<v8::Local<v8::Value>> node_attributes_values;

    for (const Attribute& attribute : element->Attributes()) {
      node_attributes_keys.push_back(
          V8String(isolate_, attribute.GetName().ToString()));
      node_attributes_values.push_back(V8String(isolate_, attribute.Value()));
    }

    DCHECK(node_attributes_values.size() == node_attributes_keys.size());
    v8::Local<v8::Object> node_attributes = v8::Object::New(
        isolate_, v8::Null(isolate_), node_attributes_keys.data(),
        node_attributes_values.data(), node_attributes_keys.size());

    serialized_value_keys.push_back(V8String(isolate_, kAttributes));
    serialized_value_values.push_back(node_attributes);
  }

  if (max_depth > 0) {
    NodeList* child_nodes = node->childNodes();

    v8::Local<v8::Array> children =
        v8::Array::New(isolate_, child_nodes->length());

    for (unsigned int i = 0; i < child_nodes->length(); i++) {
      Node* child_node = child_nodes->item(i);
      v8::Local<v8::Object> serialized_child_node =
          SerializeNodeToV8Object(child_node, isolate_, max_depth - 1);

      children
          ->CreateDataProperty(isolate_->GetCurrentContext(), i,
                               serialized_child_node)
          .Check();
    }
    serialized_value_keys.push_back(V8String(isolate_, kChildren));
    serialized_value_values.push_back(children);
  }

  DCHECK(serialized_value_values.size() == serialized_value_keys.size());

  v8::Local<v8::Object> serialized_value = v8::Object::New(
      isolate_, v8::Null(isolate_), serialized_value_keys.data(),
      serialized_value_values.data(), serialized_value_keys.size());

  Vector<v8::Local<v8::Name>> result_keys;
  Vector<v8::Local<v8::Value>> result_values;

  result_keys.push_back(TypeStringKey(isolate_));
  result_values.push_back(V8String(isolate_, kNode));

  result_keys.push_back(ValueStringKey(isolate_));
  result_values.push_back(serialized_value);

  return v8::Object::New(isolate_, v8::Null(isolate_), result_keys.data(),
                         result_values.data(), result_keys.size());
}

std::unique_ptr<v8_inspector::WebDriverValue> SerializeNodeToWebDriverValue(
    Node* node,
    v8::Isolate* isolate_,
    int max_depth) {
  v8::Local<v8::Object> node_v8_object =
      SerializeNodeToV8Object(node, isolate_, max_depth);

  v8::Local<v8::Value> value_v8_object =
      node_v8_object
          ->Get(isolate_->GetCurrentContext(), ValueStringKey(isolate_))
          .ToLocalChecked();

  // Safely get `type` from object value.
  v8::MaybeLocal<v8::Value> maybe_type_v8_value = node_v8_object->Get(
      isolate_->GetCurrentContext(), TypeStringKey(isolate_));
  DCHECK(!maybe_type_v8_value.IsEmpty());
  v8::Local<v8::Value> type_v8_value = maybe_type_v8_value.ToLocalChecked();
  DCHECK(type_v8_value->IsString());
  v8::Local<v8::String> type_v8_string = type_v8_value.As<v8::String>();
  String type_string = ToCoreString(type_v8_string);
  StringView type_string_view = StringView(type_string);
  std::unique_ptr<v8_inspector::StringBuffer> type_string_buffer =
      ToV8InspectorStringBuffer(type_string_view);

  return std::make_unique<v8_inspector::WebDriverValue>(
      std::move(type_string_buffer), value_v8_object);
}
}  // namespace

std::unique_ptr<v8_inspector::WebDriverValue>
ThreadDebuggerCommonImpl::serializeToWebDriverValue(
    v8::Local<v8::Value> v8_value,
    int max_depth) {
  // Serialize according to https://w3c.github.io/webdriver-bidi.
  if (V8Node::HasInstance(v8_value, isolate_)) {
    Node* node = V8Node::ToImplWithTypeCheck(isolate_, v8_value);
    return SerializeNodeToWebDriverValue(node, isolate_, max_depth);
  }

  if (V8Window::HasInstance(v8_value, isolate_)) {
    return std::make_unique<v8_inspector::WebDriverValue>(
        ToV8InspectorStringBuffer("window"));
  }

  if (V8DOMWrapper::IsWrapper(isolate_, v8_value)) {
    return std::make_unique<v8_inspector::WebDriverValue>(
        ToV8InspectorStringBuffer("platformobject"));
  }

  return nullptr;
}

std::unique_ptr<v8_inspector::StringBuffer>
ThreadDebuggerCommonImpl::valueSubtype(v8::Local<v8::Value> value) {
  static const char kNode[] = "node";
  static const char kArray[] = "array";
  static const char kError[] = "error";
  static const char kBlob[] = "blob";
  static const char kTrustedType[] = "trustedtype";

  if (V8Node::HasInstance(value, isolate_))
    return ToV8InspectorStringBuffer(kNode);
  if (V8NodeList::HasInstance(value, isolate_) ||
      V8DOMTokenList::HasInstance(value, isolate_) ||
      V8HTMLCollection::HasInstance(value, isolate_) ||
      V8HTMLAllCollection::HasInstance(value, isolate_)) {
    return ToV8InspectorStringBuffer(kArray);
  }
  if (V8DOMException::HasInstance(value, isolate_))
    return ToV8InspectorStringBuffer(kError);
  if (V8Blob::HasInstance(value, isolate_))
    return ToV8InspectorStringBuffer(kBlob);
  if (V8TrustedHTML::HasInstance(value, isolate_) ||
      V8TrustedScript::HasInstance(value, isolate_) ||
      V8TrustedScriptURL::HasInstance(value, isolate_)) {
    return ToV8InspectorStringBuffer(kTrustedType);
  }
  return nullptr;
}

std::unique_ptr<v8_inspector::StringBuffer>
ThreadDebuggerCommonImpl::descriptionForValueSubtype(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value) {
  if (V8TrustedHTML::HasInstance(value, isolate_)) {
    TrustedHTML* trusted_html =
        V8TrustedHTML::ToImplWithTypeCheck(isolate_, value);
    return ToV8InspectorStringBuffer(trusted_html->toString());
  } else if (V8TrustedScript::HasInstance(value, isolate_)) {
    TrustedScript* trusted_script =
        V8TrustedScript::ToImplWithTypeCheck(isolate_, value);
    return ToV8InspectorStringBuffer(trusted_script->toString());
  } else if (V8TrustedScriptURL::HasInstance(value, isolate_)) {
    TrustedScriptURL* trusted_script_url =
        V8TrustedScriptURL::ToImplWithTypeCheck(isolate_, value);
    return ToV8InspectorStringBuffer(trusted_script_url->toString());
  } else if (V8Node::HasInstance(value, isolate_)) {
    Node* node = V8Node::ToImplWithTypeCheck(isolate_, value);
    StringBuilder description;
    switch (node->getNodeType()) {
      case Node::kElementNode: {
        const auto* element = To<blink::Element>(node);
        description.Append(element->TagQName().ToString());

        const AtomicString& id = element->GetIdAttribute();
        if (!id.empty()) {
          description.Append('#');
          description.Append(id);
        }
        if (element->HasClass()) {
          auto element_class_names = element->ClassNames();
          auto n_classes = element_class_names.size();
          for (unsigned i = 0; i < n_classes; ++i) {
            description.Append('.');
            description.Append(element_class_names[i]);
          }
        }
        break;
      }
      case Node::kDocumentTypeNode: {
        description.Append("<!DOCTYPE ");
        description.Append(node->nodeName());
        description.Append('>');
        break;
      }
      default: {
        description.Append(node->nodeName());
        break;
      }
    }
    DCHECK(description.length());

    return ToV8InspectorStringBuffer(description.ToString());
  }
  return nullptr;
}

double ThreadDebuggerCommonImpl::currentTimeMS() {
  return base::Time::Now().ToDoubleT() * 1000.0;
}

bool ThreadDebuggerCommonImpl::isInspectableHeapObject(
    v8::Local<v8::Object> object) {
  if (object->InternalFieldCount() < kV8DefaultWrapperInternalFieldCount)
    return true;
  v8::Local<v8::Value> wrapper =
      object->GetInternalField(kV8DOMWrapperObjectIndex);
  // Skip wrapper boilerplates which are like regular wrappers but don't have
  // native object.
  if (!wrapper.IsEmpty() && wrapper->IsUndefined())
    return false;
  return true;
}

static void ReturnDataCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.Data());
}

static v8::Maybe<bool> CreateDataProperty(v8::Local<v8::Context> context,
                                          v8::Local<v8::Object> object,
                                          v8::Local<v8::Name> key,
                                          v8::Local<v8::Value> value) {
  v8::TryCatch try_catch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throw_js(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return object->CreateDataProperty(context, key, value);
}

static void CreateFunctionPropertyWithData(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const char* name,
    v8::FunctionCallback callback,
    v8::Local<v8::Value> data,
    const char* description,
    v8::SideEffectType side_effect_type) {
  v8::Local<v8::String> func_name = V8String(context->GetIsolate(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, data, 0,
                         v8::ConstructorBehavior::kThrow, side_effect_type)
           .ToLocal(&func))
    return;
  func->SetName(func_name);
  v8::Local<v8::String> return_value =
      V8String(context->GetIsolate(), description);
  v8::Local<v8::Function> to_string_function;
  if (v8::Function::New(context, ReturnDataCallback, return_value, 0,
                        v8::ConstructorBehavior::kThrow,
                        v8::SideEffectType::kHasNoSideEffect)
          .ToLocal(&to_string_function))
    CreateDataProperty(context, func,
                       V8AtomicString(context->GetIsolate(), "toString"),
                       to_string_function);
  CreateDataProperty(context, object, func_name, func);
}

v8::Maybe<bool> ThreadDebuggerCommonImpl::CreateDataPropertyInArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> array,
    int index,
    v8::Local<v8::Value> value) {
  v8::TryCatch try_catch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throw_js(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return array->CreateDataProperty(context, index, value);
}

void ThreadDebuggerCommonImpl::CreateFunctionProperty(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const char* name,
    v8::FunctionCallback callback,
    const char* description,
    v8::SideEffectType side_effect_type) {
  CreateFunctionPropertyWithData(context, object, name, callback,
                                 v8::External::New(context->GetIsolate(), this),
                                 description, side_effect_type);
}

void ThreadDebuggerCommonImpl::installAdditionalCommandLineAPI(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object) {
  CreateFunctionProperty(
      context, object, "getEventListeners",
      ThreadDebuggerCommonImpl::GetEventListenersCallback,
      "function getEventListeners(node) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);

  CreateFunctionProperty(
      context, object, "getAccessibleName",
      ThreadDebuggerCommonImpl::GetAccessibleNameCallback,
      "function getAccessibleName(node) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);

  CreateFunctionProperty(
      context, object, "getAccessibleRole",
      ThreadDebuggerCommonImpl::GetAccessibleRoleCallback,
      "function getAccessibleRole(node) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);

  ScriptEvaluationResult result =
      ClassicScript::CreateUnspecifiedScript(
          "(function(e) { console.log(e.type, e); })",
          ScriptSourceLocationType::kInternal)
          ->RunScriptOnScriptStateAndReturnValue(ScriptState::From(context));
  if (result.GetResultType() != ScriptEvaluationResult::ResultType::kSuccess) {
    // On pages where scripting is disabled or CSP sandbox directive is used,
    // this can be blocked and thus early exited here.
    // This is probably OK because `monitorEvents()` console API is anyway not
    // working on such pages. For more discussion see
    // https://crrev.com/c/3258735/9/third_party/blink/renderer/core/inspector/thread_debugger.cc#529
    return;
  }

  v8::Local<v8::Value> function_value = result.GetSuccessValue();
  DCHECK(function_value->IsFunction());
  CreateFunctionPropertyWithData(
      context, object, "monitorEvents",
      ThreadDebuggerCommonImpl::MonitorEventsCallback, function_value,
      "function monitorEvents(object, [types]) { [Command Line API] }",
      v8::SideEffectType::kHasSideEffect);
  CreateFunctionPropertyWithData(
      context, object, "unmonitorEvents",
      ThreadDebuggerCommonImpl::UnmonitorEventsCallback, function_value,
      "function unmonitorEvents(object, [types]) { [Command Line API] }",
      v8::SideEffectType::kHasSideEffect);
}

static Vector<String> NormalizeEventTypes(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Vector<String> types;
  if (info.Length() > 1 && info[1]->IsString())
    types.push_back(ToCoreString(info[1].As<v8::String>()));
  if (info.Length() > 1 && info[1]->IsArray()) {
    v8::Local<v8::Array> types_array = v8::Local<v8::Array>::Cast(info[1]);
    for (wtf_size_t i = 0; i < types_array->Length(); ++i) {
      v8::Local<v8::Value> type_value;
      if (!types_array->Get(info.GetIsolate()->GetCurrentContext(), i)
               .ToLocal(&type_value) ||
          !type_value->IsString())
        continue;
      types.push_back(ToCoreString(v8::Local<v8::String>::Cast(type_value)));
    }
  }
  if (info.Length() == 1)
    types.AppendVector(
        Vector<String>({"mouse",   "key",          "touch",
                        "pointer", "control",      "load",
                        "unload",  "abort",        "error",
                        "select",  "input",        "change",
                        "submit",  "reset",        "focus",
                        "blur",    "resize",       "scroll",
                        "search",  "devicemotion", "deviceorientation"}));

  Vector<String> output_types;
  for (wtf_size_t i = 0; i < types.size(); ++i) {
    if (types[i] == "mouse")
      output_types.AppendVector(
          Vector<String>({"auxclick", "click", "dblclick", "mousedown",
                          "mouseeenter", "mouseleave", "mousemove", "mouseout",
                          "mouseover", "mouseup", "mouseleave", "mousewheel"}));
    else if (types[i] == "key")
      output_types.AppendVector(
          Vector<String>({"keydown", "keyup", "keypress", "textInput"}));
    else if (types[i] == "touch")
      output_types.AppendVector(Vector<String>(
          {"touchstart", "touchmove", "touchend", "touchcancel"}));
    else if (types[i] == "pointer")
      output_types.AppendVector(Vector<String>(
          {"pointerover", "pointerout", "pointerenter", "pointerleave",
           "pointerdown", "pointerup", "pointermove", "pointercancel",
           "gotpointercapture", "lostpointercapture"}));
    else if (types[i] == "control")
      output_types.AppendVector(
          Vector<String>({"resize", "scroll", "zoom", "focus", "blur", "select",
                          "input", "change", "submit", "reset"}));
    else
      output_types.push_back(types[i]);
  }
  return output_types;
}

static EventTarget* FirstArgumentAsEventTarget(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return nullptr;
  if (EventTarget* target =
          V8EventTarget::ToImplWithTypeCheck(info.GetIsolate(), info[0]))
    return target;
  return ToDOMWindow(info.GetIsolate(), info[0]);
}

void ThreadDebuggerCommonImpl::SetMonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    bool enabled) {
  EventTarget* event_target = FirstArgumentAsEventTarget(info);
  if (!event_target)
    return;
  Vector<String> types = NormalizeEventTypes(info);
  DCHECK(!info.Data().IsEmpty() && info.Data()->IsFunction());
  V8EventListener* event_listener =
      V8EventListener::Create(info.Data().As<v8::Function>());
  for (wtf_size_t i = 0; i < types.size(); ++i) {
    if (enabled)
      event_target->addEventListener(AtomicString(types[i]), event_listener);
    else
      event_target->removeEventListener(AtomicString(types[i]), event_listener);
  }
}

// static
void ThreadDebuggerCommonImpl::MonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SetMonitorEventsCallback(info, true);
}

// static
void ThreadDebuggerCommonImpl::UnmonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SetMonitorEventsCallback(info, false);
}

// static
void ThreadDebuggerCommonImpl::GetAccessibleNameCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value = info[0];

  Node* node = V8Node::ToImplWithTypeCheck(isolate, value);
  if (node && !node->GetLayoutObject())
    return;
  if (auto* element = DynamicTo<Element>(node)) {
    V8SetReturnValueString(info, element->computedName(), isolate);
  }
}

// static
void ThreadDebuggerCommonImpl::GetAccessibleRoleCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value = info[0];

  Node* node = V8Node::ToImplWithTypeCheck(isolate, value);
  if (node && !node->GetLayoutObject())
    return;
  if (auto* element = DynamicTo<Element>(node)) {
    V8SetReturnValueString(info, element->computedRole(), isolate);
  }
}

// static
void ThreadDebuggerCommonImpl::GetEventListenersCallback(
    const v8::FunctionCallbackInfo<v8::Value>& callback_info) {
  if (callback_info.Length() < 1)
    return;

  ThreadDebuggerCommonImpl* debugger = static_cast<ThreadDebuggerCommonImpl*>(
      v8::Local<v8::External>::Cast(callback_info.Data())->Value());
  DCHECK(debugger);
  v8::Isolate* isolate = callback_info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  int group_id = debugger->ContextGroupId(ToExecutionContext(context));

  V8EventListenerInfoList listener_info;
  // eventListeners call can produce message on ErrorEvent during lazy event
  // listener compilation.
  if (group_id)
    debugger->muteMetrics(group_id);
  InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
      isolate, callback_info[0], &listener_info);
  if (group_id)
    debugger->unmuteMetrics(group_id);

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  AtomicString current_event_type;
  v8::Local<v8::Array> listeners;
  wtf_size_t output_index = 0;
  for (auto& info : listener_info) {
    if (current_event_type != info.event_type) {
      current_event_type = info.event_type;
      listeners = v8::Array::New(isolate);
      output_index = 0;
      CreateDataProperty(context, result,
                         V8AtomicString(isolate, current_event_type),
                         listeners);
    }

    v8::Local<v8::Object> listener_object = v8::Object::New(isolate);
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "listener"), info.handler);
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "useCapture"),
                       v8::Boolean::New(isolate, info.use_capture));
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "passive"),
                       v8::Boolean::New(isolate, info.passive));
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "once"),
                       v8::Boolean::New(isolate, info.once));
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "type"),
                       V8String(isolate, current_event_type));
    CreateDataPropertyInArray(context, listeners, output_index++,
                              listener_object);
  }
  callback_info.GetReturnValue().Set(result);
}

void ThreadDebuggerCommonImpl::consoleTime(
    const v8_inspector::StringView& title) {
  // TODO(dgozman): we can save on a copy here if trace macro would take a
  // pointer with length.
  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN0(
      "blink.console", ToCoreString(title).Utf8().c_str(), this);
}

void ThreadDebuggerCommonImpl::consoleTimeEnd(
    const v8_inspector::StringView& title) {
  // TODO(dgozman): we can save on a copy here if trace macro would take a
  // pointer with length.
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END0(
      "blink.console", ToCoreString(title).Utf8().c_str(), this);
}

void ThreadDebuggerCommonImpl::consoleTimeStamp(
    const v8_inspector::StringView& title) {
  ExecutionContext* ec = CurrentExecutionContext(isolate_);
  // TODO(dgozman): we can save on a copy here if TracedValue would take a
  // StringView.
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "TimeStamp", inspector_time_stamp_event::Data, ec, ToCoreString(title));
  probe::ConsoleTimeStamp(ec, ToCoreString(title));
}

void ThreadDebuggerCommonImpl::startRepeatingTimer(
    double interval,
    V8InspectorClient::TimerCallback callback,
    void* data) {
  timer_data_.push_back(data);
  timer_callbacks_.push_back(callback);

  std::unique_ptr<TaskRunnerTimer<ThreadDebuggerCommonImpl>> timer =
      std::make_unique<TaskRunnerTimer<ThreadDebuggerCommonImpl>>(
          ThreadScheduler::Current()->V8TaskRunner(), this,
          &ThreadDebuggerCommonImpl::OnTimer);
  TaskRunnerTimer<ThreadDebuggerCommonImpl>* timer_ptr = timer.get();
  timers_.push_back(std::move(timer));
  timer_ptr->StartRepeating(base::Seconds(interval), FROM_HERE);
}

void ThreadDebuggerCommonImpl::cancelTimer(void* data) {
  for (wtf_size_t index = 0; index < timer_data_.size(); ++index) {
    if (timer_data_[index] == data) {
      timers_[index]->Stop();
      timer_callbacks_.EraseAt(index);
      timers_.EraseAt(index);
      timer_data_.EraseAt(index);
      return;
    }
  }
}

int64_t ThreadDebuggerCommonImpl::generateUniqueId() {
  int64_t result;
  base::RandBytes(&result, sizeof result);
  return result;
}

void ThreadDebuggerCommonImpl::OnTimer(TimerBase* timer) {
  for (wtf_size_t index = 0; index < timers_.size(); ++index) {
    if (timers_[index].get() == timer) {
      timer_callbacks_[index](timer_data_[index]);
      return;
    }
  }
}

}  // namespace blink
