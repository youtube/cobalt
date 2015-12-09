/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/script/javascriptcore/jsc_debugger.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/jsc_global_object_proxy.h"

#include "third_party/WebKit/Source/JavaScriptCore/debugger/DebuggerCallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/heap/MarkedBlock.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/CallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceProvider.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Executable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSCell.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSFunction.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"

namespace cobalt {
namespace script {

// Static factory method declared in public interface.
scoped_ptr<JavaScriptDebuggerInterface>
JavaScriptDebuggerInterface::CreateDebugger(
    GlobalObjectProxy* global_object_proxy,
    const OnEventCallback& on_event_callback,
    const OnDetachCallback& on_detach_callback) {
  return scoped_ptr<JavaScriptDebuggerInterface>(
      new javascriptcore::JSCDebugger(global_object_proxy, on_event_callback,
                                      on_detach_callback));
}

namespace javascriptcore {

namespace {
// Event "methods" (names) from the set here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger
const char kScriptFailedToParse[] = "Debugger.scriptFailedToParse";
const char kScriptParsed[] = "Debugger.scriptParsed";
}  // namespace

JSCDebugger::JSCDebugger(GlobalObjectProxy* global_object_proxy,
                         const OnEventCallback& on_event_callback,
                         const OnDetachCallback& on_detach_callback)
    : global_object_proxy_(global_object_proxy),
      on_event_callback_(on_event_callback),
      on_detach_callback_(on_detach_callback) {
  attach(GetGlobalObject());
}

JSCDebugger::~JSCDebugger() { detach(GetGlobalObject()); }

scoped_ptr<base::DictionaryValue> JSCDebugger::GetScriptSource(
    const scoped_ptr<base::DictionaryValue>& params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);

  // Get the scriptId from the parameters object.
  // scriptId is a long number (intptr_t) stored in the JSON params as a
  // string. Assume we can hold it in an int64, as string_number_conversions
  // does not directly support intptr_t.
  DCHECK(sizeof(int64) >= sizeof(intptr_t));
  std::string script_id_string = "0";
  params->GetString("scriptId", &script_id_string);
  int64 script_id_as_int64 = 0;
  base::StringToInt64(script_id_string, &script_id_as_int64);
  intptr_t script_id = static_cast<intptr_t>(script_id_as_int64);

  // Search the set of source providers for a matching scriptId.
  JSC::SourceProvider* source_provider = NULL;
  for (SourceProviderSet::iterator iter = source_providers_.begin();
       iter != source_providers_.end(); ++iter) {
    if ((*iter)->asID() == script_id) {
      source_provider = *iter;
      break;
    }
  }
  if (source_provider) {
    response->SetString("result.scriptSource",
                        source_provider->source().latin1().data());
  } else {
    response->SetString("error.message",
                        "No source provider found with specified scriptId");
  }

  return response.Pass();
}

void JSCDebugger::GathererFunctor::operator()(JSC::JSCell* cell) {
  JSC::JSFunction* function = JSC::jsDynamicCast<JSC::JSFunction*>(cell);
  if (function && function->scope()->globalObject() == global_object_ &&
      function->executable()->isFunctionExecutable() &&
      !function->isHostFunction()) {
    source_providers_->add(
        JSC::jsCast<JSC::FunctionExecutable*>(function->executable())
            ->source()
            .provider());
  }
}

JSC::JSGlobalObject* JSCDebugger::GetGlobalObject() const {
  return base::polymorphic_downcast<JSCGlobalObjectProxy*>(global_object_proxy_)
      ->global_object();
}

void JSCDebugger::attach(JSC::JSGlobalObject* global_object) {
  DCHECK(global_object);
  JSC::Debugger::attach(global_object);

  // Gather the source providers and call sourceParsed() on each one.
  GatherSourceProviders(global_object);
  for (SourceProviderSet::iterator iter = source_providers_.begin();
       iter != source_providers_.end(); ++iter) {
    sourceParsed(global_object->globalExec(), *iter, -1, String());
  }
}

void JSCDebugger::detach(JSC::JSGlobalObject* global_object) {
  DCHECK(global_object);
  JSC::Debugger::detach(global_object);
  const std::string reason = "canceled_by_user";
  on_detach_callback_.Run(reason);
}

void JSCDebugger::sourceParsed(JSC::ExecState* exec_state,
                               JSC::SourceProvider* source_provider,
                               int error_line,
                               const WTF::String& error_message) {
  UNREFERENCED_PARAMETER(exec_state);

  // Build the notification parameters.
  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());

  // Assume we can fit the scriptId in an int64. SourceProvider stores it as an
  // intptr_t, but string_number_conversions doesn't directly support that.
  DCHECK(sizeof(int64) >= sizeof(intptr_t));
  int64 script_id = static_cast<int64>(source_provider->asID());
  params->SetString("scriptId", base::Int64ToString(script_id));
  params->SetString("url", source_provider->url().latin1().data());

  // TODO(***REMOVED***) There are a bunch of other parameters I don't know how
  // to get from a SourceProvider: start/end line/column for inline scripts,
  // etc. Need to check whether it is possible to find these somwhere, or if
  // Chrome Dev Tools can manage without these, dummy values would suffice, etc.

  // If there was an error, add the parameters for that.
  if (error_line >= 0) {
    params->SetInteger("errorLine", error_line);
    params->SetString("errorMessage", error_message.latin1().data());
  }

  if (error_line < 0) {
    on_event_callback_.Run(kScriptParsed, params);
  } else {
    on_event_callback_.Run(kScriptFailedToParse, params);
  }
}

void JSCDebugger::exception(const JSC::DebuggerCallFrame& call_frame,
                            intptr_t source_id, int line_number,
                            int column_number, bool has_handler) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::exception";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
  DLOG(INFO) << "has_handler: " << has_handler;
}

void JSCDebugger::atStatement(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::atStatement";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::callEvent(const JSC::DebuggerCallFrame& call_frame,
                            intptr_t source_id, int line_number,
                            int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::callEvent";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::returnEvent(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::returnEvent";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::willExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                                     intptr_t source_id, int line_number,
                                     int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::willExecuteProgram";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::didExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                                    intptr_t source_id, int line_number,
                                    int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::didExecuteProgram";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::didReachBreakpoint(const JSC::DebuggerCallFrame& call_frame,
                                     intptr_t source_id, int line_number,
                                     int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::didReachBreakpoint";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::GatherSourceProviders(JSC::JSGlobalObject* global_object) {
  DCHECK(global_object);
  source_providers_.clear();
  GathererFunctor gatherer_functor(global_object, &source_providers_);
  JSC::JSGlobalData& global_data = global_object->globalData();
  global_data.heap.objectSpace().forEachLiveCell(gatherer_functor);
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
