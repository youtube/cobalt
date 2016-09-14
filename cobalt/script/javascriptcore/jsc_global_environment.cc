/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/script/javascriptcore/jsc_global_environment.h"

#include <algorithm>
#include <string>

#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/jsc_engine.h"
#include "cobalt/script/javascriptcore/jsc_object_handle_holder.h"
#include "cobalt/script/javascriptcore/jsc_source_code.h"
#include "cobalt/script/javascriptcore/util/exception_helpers.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/heap/StrongInlines.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/Interpreter.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Completion.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

void JSCGlobalEnvironment::PreventGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_object_);

  JSC::JSLockHolder lock(global_object_->globalData());

  JSC::JSObject* wrapper = global_object_->wrapper_factory()->GetWrapper(
      global_object_.get(), wrappable);
  global_object_->object_cache()->CacheJSObject(wrapper);
}

JSCGlobalEnvironment::~JSCGlobalEnvironment() {
  if (global_object_.get()) {
    global_object_->setReportEvalCallback(NULL);
  }
}

void JSCGlobalEnvironment::CreateGlobalObject() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(engine_->global_data());

  JSCGlobalObject* global_object = JSCGlobalObject::Create(
      engine_->global_data(), engine_->script_object_registry());
  SetGlobalObject(global_object);
}

void JSCGlobalEnvironment::AllowGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_object_);

  JSC::JSLockHolder lock(global_object_->globalData());

  JSC::JSObject* wrapper = global_object_->wrapper_factory()->GetWrapper(
      global_object_.get(), wrappable);
  global_object_->object_cache()->UncacheJSObject(wrapper);
}

void JSCGlobalEnvironment::DisableEval(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_object_);
  global_object_->setEvalEnabled(false, message.c_str());
}

void JSCGlobalEnvironment::EnableEval() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_object_);
  global_object_->setEvalEnabled(true);
}

void JSCGlobalEnvironment::Bind(const std::string& identifier,
                                const scoped_refptr<Wrappable>& impl) {
  DCHECK(thread_checker_.CalledOnValidThread());
  javascriptcore::JSCGlobalObject* jsc_global_object =
      base::polymorphic_downcast<javascriptcore::JSCGlobalEnvironment*>(this)
          ->global_object();

  JSC::JSLockHolder lock(&jsc_global_object->globalData());

  JSC::JSObject* wrapper =
      jsc_global_object->wrapper_factory()->GetWrapper(jsc_global_object, impl);

  JSC::Identifier jsc_identifier =
      JSC::Identifier(jsc_global_object->globalExec(), identifier.c_str());

  // Make sure the property we are binding doesn't already exist.
  DCHECK(!jsc_global_object->hasOwnProperty(jsc_global_object->globalExec(),
                                            jsc_identifier));

  // Add the property to the global object.
  jsc_global_object->putDirect(jsc_global_object->globalData(), jsc_identifier,
                               wrapper);
}

bool JSCGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code, std::string* out_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(global_object_->globalData());

  JSC::JSValue result;
  bool success = EvaluateScriptInternal(source_code, &result);
  if (out_result) {
    if (!success) {
      *out_result =
          util::GetExceptionString(global_object_->globalExec(), result);
    } else {
      WTF::String return_string =
          result.toWTFString(global_object_->globalExec());
      *out_result = return_string.utf8().data();
    }
  }

  return success;
}

bool JSCGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code,
    const scoped_refptr<Wrappable>& owning_object,
    base::optional<OpaqueHandleHolder::Reference>* out_opaque_handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(global_object_->globalData());

  JSC::JSValue result;
  if (!EvaluateScriptInternal(source_code, &result)) {
    std::string exception =
        util::GetExceptionString(global_object_->globalExec(), result);
    DLOG(ERROR) << exception;
    return false;
  }
  JSCObjectHandle jsc_object_handle(JSC::asObject(result));
  JSCObjectHandleHolder jsc_object_holder(
      JSCObjectHandle(JSC::asObject(result)),
      global_object_->script_object_registry());
  out_opaque_handle->emplace(owning_object.get(), jsc_object_holder);
  return true;
}

std::vector<StackFrame> JSCGlobalEnvironment::GetStackTrace(int max_frames) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::GetStackTrace(global_object_->globalExec(), max_frames);
}

void JSCGlobalEnvironment::reportEval() {
  if (!report_eval_cb_.is_null()) {
    report_eval_cb_.Run();
  }
}

void JSCGlobalEnvironment::SetGlobalObject(JSCGlobalObject* jsc_global_object) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(reinterpret_cast<uintptr_t>(global_object_.get()), NULL);
  global_object_ =
      JSC::Strong<JSCGlobalObject>(*engine_->global_data(), jsc_global_object);
  global_object_->setReportEvalCallback(this);
  // Disable eval() unless it's explicitly enabled (by CSP, for example).
  DisableEval("eval() is disabled by default.");
}

bool JSCGlobalEnvironment::EvaluateScriptInternal(
    const scoped_refptr<SourceCode>& source_code, JSC::JSValue* out_result) {
  TRACE_EVENT0("cobalt::script::javascriptcore",
               "JSCGlobalEnvironment::EvaluateScriptInternal");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_object_);

  JSC::ExecState* exec = global_object_->globalExec();
  // Downcast to the JSC implementation of the SourceCode interface.
  JSCSourceCode* jsc_source_code =
      base::polymorphic_downcast<JSCSourceCode*>(source_code.get());

  // Evaluate the source code and gather the return value and exception if
  // one occurred.
  JSC::JSValue exception;
  JSC::JSValue return_value = JSC::evaluate(exec, jsc_source_code->source(),
                                            JSC::JSValue(),  // thisValue
                                            &exception);
  if (exception) {
    *out_result = exception;
    exec->clearException();
    return false;
  } else {
    *out_result = return_value;
    return true;
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
