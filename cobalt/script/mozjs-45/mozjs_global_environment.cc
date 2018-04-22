// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/script/mozjs-45/mozjs_global_environment.h"

#include <algorithm>
#include <utility>

#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/mozjs-45/conversion_helpers.h"
#include "cobalt/script/mozjs-45/embedded_resources.h"
#include "cobalt/script/mozjs-45/mozjs_exception_state.h"
#include "cobalt/script/mozjs-45/mozjs_script_value_factory.h"
#include "cobalt/script/mozjs-45/mozjs_source_code.h"
#include "cobalt/script/mozjs-45/mozjs_wrapper_handle.h"
#include "cobalt/script/mozjs-45/proxy_handler.h"
#include "cobalt/script/mozjs-45/referenced_object_map.h"
#include "cobalt/script/mozjs-45/util/exception_helpers.h"
#include "cobalt/script/mozjs-45/util/stack_trace_helpers.h"
#include "nb/memory_scope.h"
#include "third_party/mozjs-45/js/public/Initialization.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jsfriendapi.h"
#include "third_party/mozjs-45/js/src/jsfun.h"
#include "third_party/mozjs-45/js/src/jsobj.h"

// A note on CompartmentOptions and Principals.
// These concepts are an integral part of Gecko's security implementation, but
// we don't need to be concerned with this for Cobalt.
//
// Compartments are separate regions of memory. Each global object has a
// separate compartment. Since Cobalt will use a different JSRuntime for each
// thread and each global object cannot access another directly anyways, this
// shouldn't be an issue even in our development workflow.
// If we support multiple windows in some form, a cross-compartment wrapper is
// needed to access another global object. If they are same-origin this is
// simpler.
// Principals (or Security principals) are used to determine the security
// relationship between two compartments. Since Cobalt does not have multiple
// global objects there is no need to define Security principals. Even so,
// it we'd likely want to implement such a security policy in an engine
// independent way anyways.
// See https://developer.mozilla.org/en-US/docs/Mozilla/Gecko/Script_security

namespace cobalt {
namespace script {
namespace mozjs {

namespace {

// Class definition for global object with no bindings.
JSClass simple_global_class = {"global", JSCLASS_GLOBAL_FLAGS,
                               NULL,     NULL,
                               NULL,     NULL,
                               NULL,     NULL,
                               NULL,     NULL,
                               NULL,     NULL,
                               NULL,     JS_GlobalObjectTraceHook};

// 8192 is the recommended default value.
const uint32_t kStackChunkSize = 8192;

// This is the default in the spidermonkey shell.
const uint32_t kMaxCodeCacheBytes = 16 * 1024 * 1024;

// DOM proxies have an extra slot for the expando object at index
// kJSProxySlotExpando.
// The expando object is a plain JSObject whose properties correspond to
// "expandos" (custom properties set by the script author).
// The exact value stored in the kJSProxySlotExpando slot depends on whether
// the interface is annotated with the [OverrideBuiltins] extended attribute.
const uint32_t kJSProxySlotExpando = 0;

// The DOMProxyShadowsCheck function will be called to check if the property for
// id should be gotten from the prototype, or if there is an own property that
// shadows it.
js::DOMProxyShadowsResult DOMProxyShadowsCheck(JSContext* context,
                                               JS::HandleObject proxy,
                                               JS::HandleId id) {
  DCHECK(IsProxy(proxy));
  JS::Value value = js::GetProxyExtra(proxy, kJSProxySlotExpando);
  DCHECK(value.isUndefined() || value.isObject());

  // [OverrideBuiltins] extended attribute is not supported.
  NOTIMPLEMENTED();

  // If DoesntShadow is returned then the slot at listBaseExpandoSlot should
  // either be undefined or point to an expando object that would contain the
  // own property.
  return js::DoesntShadow;
}

class MozjsStubHandler : public ProxyHandler {
 public:
  MozjsStubHandler()
      : ProxyHandler(indexed_property_hooks, named_property_hooks) {}

 private:
  static NamedPropertyHooks named_property_hooks;
  static IndexedPropertyHooks indexed_property_hooks;
};

ProxyHandler::NamedPropertyHooks MozjsStubHandler::named_property_hooks = {
    NULL, NULL, NULL, NULL, NULL,
};
ProxyHandler::IndexedPropertyHooks MozjsStubHandler::indexed_property_hooks = {
    NULL, NULL, NULL, NULL, NULL,
};

static base::LazyInstance<MozjsStubHandler> proxy_handler;

}  // namespace

// static
MozjsGlobalEnvironment* MozjsGlobalEnvironment::GetFromContext(
    JSContext* context) {
  MozjsGlobalEnvironment* global_proxy =
      static_cast<MozjsGlobalEnvironment*>(JS_GetContextPrivate(context));
  DCHECK(global_proxy);
  return global_proxy;
}

// static
bool MozjsGlobalEnvironment::CheckEval(JSContext* context) {
  TRACK_MEMORY_SCOPE("Javascript");
  MozjsGlobalEnvironment* global_environment = GetFromContext(context);
  DCHECK(global_environment);
  if (!global_environment->report_eval_.is_null()) {
    global_environment->report_eval_.Run();
  }
  return global_environment->eval_enabled_;
}

MozjsGlobalEnvironment::MozjsGlobalEnvironment(JSRuntime* runtime)
    : context_(NULL),
      garbage_collection_count_(0),
      context_destructor_(&context_),
      environment_settings_(NULL),
      last_error_message_(NULL),
      eval_enabled_(false) {
  TRACK_MEMORY_SCOPE("Javascript");
  context_ = JS_NewContext(runtime, kStackChunkSize);
  DCHECK(context_);
  // Set a pointer to this class inside the JSContext.
  JS_SetContextPrivate(context_, this);

#if defined(COBALT_GC_ZEAL)
  // Set zeal level to "Collect when every frequency allocations." See
  // https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_SetGCZeal
  // for other valid options.
  const uint8_t kZealLevel = 2;
  const uint32_t kZealFrequency = 600;
  JS_SetGCZeal(context_, kZealLevel, kZealFrequency);
#endif

  JS_SetGCParameterForThread(context_, JSGC_MAX_CODE_CACHE_BYTES,
                             kMaxCodeCacheBytes);

  wrapper_factory_.reset(new WrapperFactory(context_));
  script_value_factory_.reset(new MozjsScriptValueFactory(this));
  referenced_objects_.reset(new ReferencedObjectMap(context_));

  JS_AddExtraGCRootsTracer(runtime, TraceFunction, this);
}

MozjsGlobalEnvironment::~MozjsGlobalEnvironment() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JS_RemoveExtraGCRootsTracer(JS_GetRuntime(context_), TraceFunction, this);
}

void MozjsGlobalEnvironment::CreateGlobalObject() {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!global_object_proxy_);

  // The global object is automatically rooted unless the
  // JSOPTION_UNROOTED_GLOBAL option is set.
  JSAutoRequest auto_request(context_);

  // NULL JSPrincipals and default JS::CompartmentOptions. See the comment
  // above for rationale.
  JS::RootedObject global_object(
      context_, JS_NewGlobalObject(context_, &simple_global_class, NULL,
                                   JS::FireOnNewGlobalHook));
  DCHECK(global_object);

  // Initialize standard JS constructors prototypes and top-level functions such
  // as Object, isNan, etc.
  JSAutoCompartment auto_compartment(context_, global_object);
  bool success = JS_InitStandardClasses(context_, global_object);
  DCHECK(success);

  JS::RootedObject proxy(
      context_, ProxyHandler::NewProxy(context_, proxy_handler.Pointer(),
                                       global_object, NULL));
  global_object_proxy_ = proxy;

  EvaluateAutomatics();
}

bool MozjsGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code,
    std::string* out_result_utf8) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  JSAutoRequest auto_request(context_);
  JSAutoCompartment auto_compartment(context_, global_object_proxy_);
  JS::AutoSaveExceptionState auto_save_exception_state(context_);
  JS::RootedValue result_value(context_);

  std::string error_message;
  last_error_message_ = &error_message;

  bool success = EvaluateScriptInternal(source_code, &result_value);
  if (out_result_utf8) {
    if (success) {
      MozjsExceptionState exception_state(context_);
      FromJSValue(context_, result_value, kNoConversionFlags, &exception_state,
                  out_result_utf8);
    } else if (last_error_message_) {
      *out_result_utf8 = *last_error_message_;
    } else {
      DLOG(ERROR) << "Script execution failed.";
    }
  }
  last_error_message_ = NULL;
  return success;
}

bool MozjsGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code,
    const scoped_refptr<Wrappable>& owning_object,
    base::optional<ValueHandleHolder::Reference>* out_value_handle) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());
  JSAutoRequest auto_request(context_);
  JSAutoCompartment auto_compartment(context_, global_object_proxy_);
  JS::AutoSaveExceptionState auto_save_exception_state(context_);
  JS::RootedValue result_value(context_);
  bool success = EvaluateScriptInternal(source_code, &result_value);
  if (success && out_value_handle) {
    MozjsValueHandleHolder mozjs_value_holder(context_, result_value);
    out_value_handle->emplace(owning_object.get(), mozjs_value_holder);
  }
  return success;
}

bool MozjsGlobalEnvironment::EvaluateScriptInternal(
    const scoped_refptr<SourceCode>& source_code,
    JS::MutableHandleValue out_result) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_object_proxy_);
  MozjsSourceCode* mozjs_source_code =
      base::polymorphic_downcast<MozjsSourceCode*>(source_code.get());

  const std::string& script = mozjs_source_code->source_utf8();
  const base::SourceLocation location = mozjs_source_code->location();

  JS::RootedObject global_object(
      context_, js::GetProxyTargetObject(global_object_proxy_));

  size_t length = script.size();
  char16_t* inflated_buffer =
      UTF8CharsToNewTwoByteCharsZ(
          context_, JS::UTF8Chars(script.c_str(), length), &length)
          .get();

  if (!inflated_buffer) {
    DLOG(ERROR) << "Malformed UTF-8 script.";
    return false;
  }

  JS::CompileOptions options(context_);
  options.setFileAndLine(location.file_path.c_str(), location.line_number)
      .setMutedErrors(mozjs_source_code->is_muted());
  bool success =
      JS::Evaluate(context_, options, inflated_buffer, length, out_result);
  if (!success && context_->isExceptionPending()) {
    JS_ReportPendingException(context_);
  }
  js_free(inflated_buffer);

  return success;
}

void MozjsGlobalEnvironment::EvaluateEmbeddedScript(
    const unsigned char* data, size_t size, const char* filename) {
  TRACK_MEMORY_SCOPE("Javascript");
  std::string source(reinterpret_cast<const char*>(data), size);
  scoped_refptr<SourceCode> source_code =
      new MozjsSourceCode(source, base::SourceLocation(filename, 1, 1));
  std::string result;
  bool success = EvaluateScript(source_code, &result);
  if (!success) {
    DLOG(FATAL) << result;
  }
}

std::vector<StackFrame> MozjsGlobalEnvironment::GetStackTrace(int max_frames) {
  DCHECK(thread_checker_.CalledOnValidThread());
  nb::RewindableVector<StackFrame> stack_frames;
  util::GetStackTrace(context_, max_frames, &stack_frames);
  return stack_frames.InternalData();
}

void MozjsGlobalEnvironment::PreventGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(thread_checker_.CalledOnValidThread());

  JSAutoRequest auto_request(context_);
  JSAutoCompartment auto_compartment(context_, global_object_proxy_);
  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromWrappable(wrappable, context_, wrapper_factory());
  // Attempt to insert a |Wrappable*| -> wrapper mapping into
  // |kept_alive_objects_|...
  auto insert_result = kept_alive_objects_.insert(
      {wrappable.get(),
       {JS::Heap<JSObject*>(wrapper_private->js_object_proxy()), 1}});
  // ...and if it was already there, just increment the count.
  if (!insert_result.second) {
    insert_result.first->second.count++;
  }
}

void MozjsGlobalEnvironment::AllowGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = kept_alive_objects_.find(wrappable.get());
  DCHECK(it != kept_alive_objects_.end());
  it->second.count--;
  DCHECK_GE(it->second.count, 0);
  if (it->second.count == 0) {
    kept_alive_objects_.erase(it);
  }
}

void MozjsGlobalEnvironment::AddRoot(Traceable* traceable) {
  DCHECK(traceable);
  roots_.insert(traceable);
}

void MozjsGlobalEnvironment::RemoveRoot(Traceable* traceable) {
  DCHECK(traceable);
  auto it = roots_.find(traceable);
  DCHECK(it != roots_.end());
  roots_.erase(it);
}

void MozjsGlobalEnvironment::DisableEval(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  eval_disabled_message_.emplace(message);
  eval_enabled_ = false;
}

void MozjsGlobalEnvironment::EnableEval() {
  DCHECK(thread_checker_.CalledOnValidThread());
  eval_disabled_message_ = base::nullopt;
  eval_enabled_ = true;
}

void MozjsGlobalEnvironment::DisableJit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JS::RuntimeOptionsRef(context_)
      .setBaseline(false)
      .setIon(false)
      .setAsmJS(false)
      .setNativeRegExp(false);
}

void MozjsGlobalEnvironment::SetReportEvalCallback(
    const base::Closure& report_eval) {
  DCHECK(thread_checker_.CalledOnValidThread());
  report_eval_ = report_eval;
}

void MozjsGlobalEnvironment::SetReportErrorCallback(
    const ReportErrorCallback& report_error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  report_error_callback_ = report_error_callback;
}

void MozjsGlobalEnvironment::Bind(const std::string& identifier,
                                  const scoped_refptr<Wrappable>& impl) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(impl);
  JSAutoRequest auto_request(context_);
  JSAutoCompartment auto_compartment(context_, global_object_proxy_);

  JS::RootedObject wrapper_proxy(context_,
                                 wrapper_factory_->GetWrapperProxy(impl));
  JS::RootedObject global_object(
      context_, js::GetProxyTargetObject(global_object_proxy_));

  JS::RootedValue wrapper_value(context_);
  wrapper_value.setObject(*wrapper_proxy);

  bool success = JS_SetProperty(context_, global_object, identifier.c_str(),
                                wrapper_value);

  DCHECK(success);
}

ScriptValueFactory* MozjsGlobalEnvironment::script_value_factory() {
  DCHECK(script_value_factory_);
  return script_value_factory_.get();
}

// static
void MozjsGlobalEnvironment::TraceFunction(JSTracer* js_tracer, void* data) {
  MozjsGlobalEnvironment* global_environment =
      static_cast<MozjsGlobalEnvironment*>(data);
  if (global_environment->global_object_proxy_) {
    JS_CallObjectTracer(js_tracer, &global_environment->global_object_proxy_,
                        "MozjsGlobalEnvironment");
  }

  for (auto& interface_data : global_environment->cached_interface_data_) {
    if (interface_data.prototype) {
      JS_CallObjectTracer(js_tracer, &interface_data.prototype,
                          "MozjsGlobalEnvironment");
    }
    if (interface_data.interface_object) {
      JS_CallObjectTracer(js_tracer, &interface_data.interface_object,
                          "MozjsGlobalEnvironment");
    }
  }

  auto& kept_alive_objects_ = global_environment->kept_alive_objects_;
  for (auto& pair : kept_alive_objects_) {
    auto& counted_heap_object = pair.second;
    DCHECK_GT(counted_heap_object.count, 0);
    JS_CallObjectTracer(js_tracer, &counted_heap_object.heap_object,
                        "MozjsGlobalEnvironment");
  }

  MozjsTracer mozjs_tracer(js_tracer);
  for (Traceable* root : global_environment->roots_) {
    mozjs_tracer.Trace(root);
    mozjs_tracer.DrainFrontier();
  }
}

void MozjsGlobalEnvironment::EvaluateAutomatics() {
  EvaluateEmbeddedScript(
      MozjsEmbeddedResources::promise_min_js,
      sizeof(MozjsEmbeddedResources::promise_min_js),
      "promise.min.js");
  // Store the |Promise| (currently the only polyfill that needs to be
  // accessed from native code) implemented there in our own handle (as
  // opposed to fetching it from the global object everytime we need it), in
  // order to defend ourselves from application JavaScript modifying
  // |window.Promise| later.
  {
    DCHECK(!stored_promise_constructor_);
    JS::RootedObject global_object(
        context_, js::GetProxyTargetObject(global_object_proxy_));
    JSAutoRequest auto_request(context_);
    JSAutoCompartment auto_compartment(context_, global_object);
    JS::RootedValue promise_constructor_property(context_);
    bool result = JS_GetProperty(context_, global_object, "Promise",
                                 &promise_constructor_property);
    DCHECK(result);
    DCHECK(promise_constructor_property.isObject());
    stored_promise_constructor_ = JS::PersistentRootedObject(
        context_, &promise_constructor_property.toObject());
    DCHECK(stored_promise_constructor_);
  }
  EvaluateEmbeddedScript(
      MozjsEmbeddedResources::byte_length_queuing_strategy_js,
      sizeof(MozjsEmbeddedResources::byte_length_queuing_strategy_js),
      "byte_length_queuing_strategy.js");
  EvaluateEmbeddedScript(
      MozjsEmbeddedResources::count_queuing_strategy_js,
      sizeof(MozjsEmbeddedResources::count_queuing_strategy_js),
      "count_queuing_strategy.js");
  EvaluateEmbeddedScript(
      MozjsEmbeddedResources::readable_stream_js,
      sizeof(MozjsEmbeddedResources::readable_stream_js),
      "readable_stream.js");
  EvaluateEmbeddedScript(
      MozjsEmbeddedResources::fetch_js,
      sizeof(MozjsEmbeddedResources::fetch_js),
      "fetch.js");
}

InterfaceData* MozjsGlobalEnvironment::GetInterfaceData(int key) {
  DCHECK_GE(key, 0);
  if (key >= cached_interface_data_.size()) {
    cached_interface_data_.resize(key + 1);
  }
  return &cached_interface_data_[key];
}

void MozjsGlobalEnvironment::DoSweep() {
  TRACK_MEMORY_SCOPE("Javascript");
  weak_object_manager_.SweepUnmarkedObjects();
  // Remove NULL references after sweeping weak references.
  referenced_objects_->RemoveNullReferences();
}

void MozjsGlobalEnvironment::BeginGarbageCollection() {
  TRACK_MEMORY_SCOPE("Javascript");
  // It's possible that a GC could be triggered from within the
  // BeginGarbageCollection callback. Only verify that |visisted_wrappables_|
  // is empty the first time we enter.
  garbage_collection_count_++;

  if (garbage_collection_count_ == 1) {
    DCHECK_EQ(visited_traceables_.size(), 0);
  }
}

void MozjsGlobalEnvironment::EndGarbageCollection() {
  // Reset |visisted_wrappables_|.
  garbage_collection_count_--;
  DCHECK_GE(garbage_collection_count_, 0);
  if (garbage_collection_count_ == 0) {
    visited_traceables_.clear();
  }
}

void MozjsGlobalEnvironment::SetGlobalObjectProxyAndWrapper(
    JS::HandleObject global_object_proxy,
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(!global_object_proxy_);
  DCHECK(global_object_proxy);
  DCHECK(JS_IsGlobalObject(js::GetProxyTargetObject(global_object_proxy)));
  DCHECK(IsProxy(global_object_proxy));

  global_object_proxy_ = global_object_proxy;

  // Global object cached wrapper is not set as usual object.
  // Set the global object cached wrapper, so we can get the object proxy
  // through wrapper handle.
  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromProxyObject(context_, global_object_proxy);

  DCHECK(wrapper_private);

  scoped_ptr<Wrappable::WeakWrapperHandle> object_handle(
      new MozjsWrapperHandle(wrapper_private));
  SetCachedWrapper(wrappable.get(), object_handle.Pass());
}

void MozjsGlobalEnvironment::ReportError(const char* message,
                                         JSErrorReport* report) {
  JS::RootedValue exception(context_);
  ::JS_GetPendingException(context_, &exception);

  // Note: we must do this before running any more code on context.
  ::JS_ClearPendingException(context_);

  // Populate the error report.
  ErrorReport error_report;
  if (report->errorNumber == JSMSG_CSP_BLOCKED_EVAL) {
    error_report.message = eval_disabled_message_.value_or(message);
  } else {
    error_report.message = message;
  }
  error_report.filename =
      report->filename ? report->filename : "<internal exception>";
  error_report.line_number = report->lineno;
  error_report.column_number = report->column;
  // Let error object be the object that represents the error: in the case of
  // an uncaught exception, that would be the object that was thrown; in the
  // case of a JavaScript error that would be an Error object. If there is no
  // corresponding object, then the null value must be used instead.
  //   https://www.w3.org/TR/html5/webappapis.html#runtime-script-errors
  if (exception.isObject()) {
    error_report.error.reset(new MozjsValueHandleHolder(context_, exception));
  }
  error_report.is_muted = report->isMuted;

  // If this isn't simply a warning, and the error wasn't caused by JS running
  // out of memory (in which case the callback will fail as well), then run
  // the callback. In the case that it returns that the script was handled,
  // simply return; the error should only be reported to the user if it wasn't
  // handled.
  if (!JSREPORT_IS_WARNING(report->flags) &&
      report->errorNumber != JSMSG_OUT_OF_MEMORY &&
      !report_error_callback_.is_null() &&
      report_error_callback_.Run(error_report)) {
    return;
  }

  // If the error is not handled, then the error may be reported to the user.
  //   https://www.w3.org/TR/html5/webappapis.html#runtime-script-errors-in-documents
  if (last_error_message_) {
    *last_error_message_ = error_report.message;
  } else {
    LOG(ERROR) << "JS Error: " << error_report.filename << ":"
               << error_report.line_number << ":" << error_report.column_number
               << ": " << error_report.message;
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
