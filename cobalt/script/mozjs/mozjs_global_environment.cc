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
#include "cobalt/script/mozjs/mozjs_global_environment.h"

#include <algorithm>
#include <utility>

#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/mozjs/conversion_helpers.h"
#include "cobalt/script/mozjs/embedded_resources.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "cobalt/script/mozjs/mozjs_script_value_factory.h"
#include "cobalt/script/mozjs/mozjs_source_code.h"
#include "cobalt/script/mozjs/mozjs_wrapper_handle.h"
#include "cobalt/script/mozjs/proxy_handler.h"
#include "cobalt/script/mozjs/referenced_object_map.h"
#include "cobalt/script/mozjs/util/exception_helpers.h"
#include "nb/memory_scope.h"
#include "third_party/mozjs/js/public/RootingAPI.h"
#include "third_party/mozjs/js/src/jsfriendapi.h"
#include "third_party/mozjs/js/src/jsfun.h"
#include "third_party/mozjs/js/src/jsobj.h"

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
JSClass simple_global_class = {
    "global",               // name
    JSCLASS_GLOBAL_FLAGS,   // flags
    JS_PropertyStub,        // addProperty
    JS_DeletePropertyStub,  // delProperty
    JS_PropertyStub,        // getProperty
    JS_StrictPropertyStub,  // setProperty
    JS_EnumerateStub,       // enumerate
    JS_ResolveStub,         // resolve
    JS_ConvertStub,         // convert
};

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

MozjsGlobalEnvironment::MozjsGlobalEnvironment(
     JSRuntime* runtime, const JavaScriptEngine::Options& options)
    : context_(NULL),
      garbage_collection_count_(0),
      cached_interface_data_deleter_(&cached_interface_data_),
      context_destructor_(&context_),
      environment_settings_(NULL),
      last_error_message_(NULL),
      eval_enabled_(false) {
  TRACK_MEMORY_SCOPE("Javascript");
  context_ = JS_NewContext(runtime, kStackChunkSize);
  DCHECK(context_);
  // Set a pointer to this class inside the JSContext.
  JS_SetContextPrivate(context_, this);

  JS_SetGCParameterForThread(context_, JSGC_MAX_CODE_CACHE_BYTES,
                             kMaxCodeCacheBytes);
  uint32_t moz_options =
      JSOPTION_TYPE_INFERENCE |
      JSOPTION_VAROBJFIX |       // Recommended to enable this in the API docs.
      JSOPTION_COMPILE_N_GO |    // Compiled scripts will be run only once.
      JSOPTION_UNROOTED_GLOBAL;  // Global handle must be visited to ensure it
                                 // is not GC'd.
#if ENGINE_SUPPORTS_JIT
  if (!options.disable_jit) {
    moz_options |= JSOPTION_BASELINE |  // Enable baseline compiler.
                   JSOPTION_ION;        // Enable IonMonkey
    // This is required by baseline and IonMonkey.
    js::SetDOMProxyInformation(0 /*domProxyHandlerFamily*/, kJSProxySlotExpando,
                               DOMProxyShadowsCheck);
  }
#endif

  JS_SetOptions(context_, moz_options);

  JS_SetErrorReporter(context_, &MozjsGlobalEnvironment::ReportErrorHandler);

  wrapper_factory_.reset(new WrapperFactory(context_));
  script_value_factory_.reset(new MozjsScriptValueFactory(this));
  referenced_objects_.reset(new ReferencedObjectMap(context_));
  opaque_root_tracker_.reset(new OpaqueRootTracker(
      context_, referenced_objects_.get(), wrapper_factory_.get()));

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
      context_, JS_NewGlobalObject(context_, &simple_global_class, NULL));
  DCHECK(global_object);

  // Initialize standard JS constructors prototypes and top-level functions such
  // as Object, isNan, etc.
  JSAutoCompartment auto_compartment(context_, global_object);
  bool success = JS_InitStandardClasses(context_, global_object);
  DCHECK(success);

  JS::RootedObject proxy(
      context_, ProxyHandler::NewProxy(context_, global_object, NULL, NULL,
                                       proxy_handler.Pointer()));
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
  JSExceptionState* previous_exception_state = JS_SaveExceptionState(context_);
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
  JS_RestoreExceptionState(context_, previous_exception_state);
  return success;
}

bool MozjsGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code,
    const scoped_refptr<Wrappable>& owning_object,
    base::optional<OpaqueHandleHolder::Reference>* out_opaque_handle) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());
  JSAutoRequest auto_request(context_);
  JSAutoCompartment auto_compartment(context_, global_object_proxy_);
  JSExceptionState* previous_exception_state = JS_SaveExceptionState(context_);
  JS::RootedValue result_value(context_);
  bool success = EvaluateScriptInternal(source_code, &result_value);
  if (success && out_opaque_handle) {
    MozjsObjectHandleHolder mozjs_object_holder(result_value, context_,
                                                wrapper_factory());
    out_opaque_handle->emplace(owning_object.get(), mozjs_object_holder);
  }
  JS_RestoreExceptionState(context_, previous_exception_state);
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
  jschar* inflated_buffer =
      js::InflateUTF8String(context_, script.c_str(), &length);
  bool success = false;
  if (inflated_buffer) {
    success = JS_EvaluateUCScript(context_, global_object, inflated_buffer,
                                  length, location.file_path.c_str(),
                                  location.line_number, out_result.address());
    js_free(inflated_buffer);
  } else {
    DLOG(ERROR) << "Malformed UTF-8 script.";
  }

  return success;
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
  JS::RootedObject proxy(context_, wrapper_private->js_object_proxy());
  JS::Heap<JSObject*> proxy_heap(proxy);
  kept_alive_objects_.insert(CachedWrapperMultiMap::value_type(
      wrappable.get(), proxy_heap));
}

void MozjsGlobalEnvironment::AllowGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());
  CachedWrapperMultiMap::iterator it =
      kept_alive_objects_.find(wrappable.get());
  DCHECK(it != kept_alive_objects_.end());
  if (it != kept_alive_objects_.end()) {
    kept_alive_objects_.erase(it);
  }
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
  uint32 current_options = JS_GetOptions(context_);
  uint32 new_options = current_options & ~(JSOPTION_BASELINE | JSOPTION_ION);
  JS_SetOptions(context_, new_options);
}

void MozjsGlobalEnvironment::SetReportEvalCallback(
    const base::Closure& report_eval) {
  DCHECK(thread_checker_.CalledOnValidThread());
  report_eval_ = report_eval;
}

void MozjsGlobalEnvironment::Bind(const std::string& identifier,
                                  const scoped_refptr<Wrappable>& impl) {
  TRACK_MEMORY_SCOPE("Javascript");
  JSAutoRequest auto_request(context_);
  JSAutoCompartment auto_compartment(context_, global_object_proxy_);

  JS::RootedObject wrapper_proxy(context_,
                                 wrapper_factory_->GetWrapperProxy(impl));
  JS::RootedObject global_object(
      context_, js::GetProxyTargetObject(global_object_proxy_));
  JS::Value wrapper_value = OBJECT_TO_JSVAL(wrapper_proxy);
  bool success = JS_SetProperty(context_, global_object, identifier.c_str(),
                                &wrapper_value);
  DCHECK(success);
}

ScriptValueFactory* MozjsGlobalEnvironment::script_value_factory() {
  DCHECK(script_value_factory_);
  return script_value_factory_.get();
}

void MozjsGlobalEnvironment::EvaluateAutomatics() {
  TRACK_MEMORY_SCOPE("Javascript");
  std::string source(
      reinterpret_cast<const char*>(MozjsEmbeddedResources::promise_min_js),
      sizeof(MozjsEmbeddedResources::promise_min_js));
  scoped_refptr<SourceCode> source_code =
      new MozjsSourceCode(source, base::SourceLocation("promise.min.js", 1, 1));
  std::string result;
  bool success = EvaluateScript(source_code, &result);
  if (!success) {
    DLOG(FATAL) << result;
  }
}

InterfaceData* MozjsGlobalEnvironment::GetInterfaceData(intptr_t key) {
  CachedInterfaceData::iterator it = cached_interface_data_.find(key);
  if (it != cached_interface_data_.end()) {
    return it->second;
  }
  return NULL;
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
  // BeginGarbageCollection callback. Only create the OpaqueRootState the first
  // time we enter.
  garbage_collection_count_++;
  if (global_object_proxy_ && garbage_collection_count_ == 1) {
    DCHECK(!opaque_root_state_);
    JSAutoRequest auto_request(context_);
    JSAutoCompartment auto_compartment(context_, global_object_proxy_);
    // Get the current state of opaque root relationships. Keep this object
    // alive for the duration of the GC phase to ensure that reachability
    // between roots and reachable objects is maintained.
    opaque_root_state_ = opaque_root_tracker_->GetCurrentOpaqueRootState();
  }
}

void MozjsGlobalEnvironment::EndGarbageCollection() {
  // Reset opaque root reachability relationships.
  garbage_collection_count_--;
  DCHECK_GE(garbage_collection_count_, 0);
  if (garbage_collection_count_ == 0) {
    opaque_root_state_.reset(NULL);
  }
}

MozjsGlobalEnvironment* MozjsGlobalEnvironment::GetFromContext(
    JSContext* context) {
  MozjsGlobalEnvironment* global_proxy =
      static_cast<MozjsGlobalEnvironment*>(JS_GetContextPrivate(context));
  DCHECK(global_proxy);
  return global_proxy;
}

void MozjsGlobalEnvironment::SetGlobalObjectProxyAndWrapper(
    JS::HandleObject global_object_proxy,
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(!global_object_proxy_);
  DCHECK(global_object_proxy);
  DCHECK(JS_IsGlobalObject(js::GetProxyTargetObject(global_object_proxy)));
  DCHECK(IsObjectProxy(global_object_proxy));

  global_object_proxy_ = global_object_proxy;

  // Global object cached wrapper is not set as usual object.
  // Set the global object cached wrapper, so we can get the object proxy
  // through wrapper handle.
  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromProxyObject(context_, global_object_proxy_);
  DCHECK(wrapper_private);

  scoped_ptr<Wrappable::WeakWrapperHandle> object_handle(
      new MozjsWrapperHandle(wrapper_private));
  SetCachedWrapper(wrappable.get(), object_handle.Pass());
}

void MozjsGlobalEnvironment::CacheInterfaceData(intptr_t key,
                                                InterfaceData* interface_data) {
  std::pair<CachedInterfaceData::iterator, bool> pib =
      cached_interface_data_.insert(std::make_pair(key, interface_data));
  DCHECK(pib.second);
}

void MozjsGlobalEnvironment::ReportErrorHandler(JSContext* context,
                                                const char* message,
                                                JSErrorReport* report) {
  MozjsGlobalEnvironment* global_object_proxy = GetFromContext(context);
  std::string error_message;
  if (report->errorNumber == JSMSG_CSP_BLOCKED_EVAL) {
    error_message =
        global_object_proxy->eval_disabled_message_.value_or(message);
  } else {
    error_message = message;
  }

  std::string message_with_location =
      base::StringPrintf("%s:%u:%u: %s",
                         (report->filename ? report->filename : "(none)"),
                         report->lineno,
                         report->column,
                         error_message.c_str());
  if (global_object_proxy && global_object_proxy->last_error_message_) {
    *(global_object_proxy->last_error_message_) = message_with_location;
  } else {
    LOG(ERROR) << "JS Error: " << message_with_location;
  }
}

void MozjsGlobalEnvironment::TraceFunction(JSTracer* trace, void* data) {
  MozjsGlobalEnvironment* global_object_environment =
      reinterpret_cast<MozjsGlobalEnvironment*>(data);
  if (global_object_environment->global_object_proxy_) {
    JS_CallHeapObjectTracer(trace,
                            &global_object_environment->global_object_proxy_,
                            "MozjsGlobalEnvironment");
  }
  for (CachedInterfaceData::iterator it =
           global_object_environment->cached_interface_data_.begin();
       it != global_object_environment->cached_interface_data_.end(); ++it) {
    InterfaceData* data = it->second;
    // Check whether prototype and interface object for this interface have been
    // created yet or not before attempting to trace them.
    if (data->prototype) {
      JS_CallHeapObjectTracer(trace, &data->prototype,
                              "MozjsGlobalEnvironment");
    }
    if (data->interface_object) {
      JS_CallHeapObjectTracer(trace, &data->interface_object,
                              "MozjsGlobalEnvironment");
    }
  }
  for (CachedWrapperMultiMap::iterator it =
           global_object_environment->kept_alive_objects_.begin();
       it != global_object_environment->kept_alive_objects_.end(); ++it) {
    JS_CallHeapObjectTracer(trace, &it->second, "MozjsGlobalEnvironment");
  }
}

JSBool MozjsGlobalEnvironment::CheckEval(JSContext* context) {
  TRACK_MEMORY_SCOPE("Javascript");
  MozjsGlobalEnvironment* global_object_proxy = GetFromContext(context);
  DCHECK(global_object_proxy);
  if (!global_object_proxy->report_eval_.is_null()) {
    global_object_proxy->report_eval_.Run();
  }
  return global_object_proxy->eval_enabled_;
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
