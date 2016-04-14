/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/javascriptcore/jsc_call_frame.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "cobalt/script/javascriptcore/jsc_scope.h"

#include "third_party/WebKit/Source/JavaScriptCore/interpreter/CallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/Interpreter.h"
#include "third_party/WebKit/Source/WTF/wtf/text/CString.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace cobalt {
namespace script {

namespace javascriptcore {

namespace {
// Conversion from intptr_t to std::string.
// Used to generate a unique string ID by converting the object pointer.
std::string IntptrToString(intptr_t input) {
  COMPILE_ASSERT(sizeof(int64) >= sizeof(intptr_t),
                 int64_not_big_enough_to_store_intptr_t);
  int64 input_as_int64 = static_cast<int64>(input);
  return base::Int64ToString(input_as_int64);
}
}  // namespace

JSCCallFrame::JSCCallFrame(const JSC::DebuggerCallFrame& call_frame,
                           intptr_t script_id, int line_number)
    : call_frame_(call_frame),
      script_id_(IntptrToString(script_id)),
      line_number_(line_number) {
  Initialize();
}

JSCCallFrame::JSCCallFrame(const JSC::DebuggerCallFrame& call_frame,
                           intptr_t script_id, int line_number,
                           int column_number)
    : call_frame_(call_frame),
      script_id_(IntptrToString(script_id)),
      line_number_(line_number),
      column_number_(column_number) {
  Initialize();
}

JSCCallFrame::~JSCCallFrame() {}

void JSCCallFrame::Initialize() {
  DCHECK(call_frame_.callFrame());
  DCHECK_GT(script_id_.length(), 0);
  global_object_ = JSC::jsCast<JSCGlobalObject*>(
      call_frame_.callFrame()->dynamicGlobalObject());
  DCHECK(global_object_);

  JSC::JSValue this_value = call_frame_.callFrame()->thisValue();
  JSC::JSObject* this_object = this_value.getObject();
  if (this_object) {
    this_holder_.reset(
        new JSCObjectHandleHolder(JSCObjectHandle(this_object),
                                  global_object_->script_object_registry()));
  } else {
    DLOG(WARNING) << "JavaScript 'this' value is not an object.";
  }
}

std::string JSCCallFrame::GetCallFrameId() {
  return IntptrToString(reinterpret_cast<intptr_t>(call_frame_.callFrame()));
}

scoped_ptr<CallFrame> JSCCallFrame::GetCaller() {
  JSC::CallFrame* call_frame = call_frame_.callFrame();
  DCHECK(call_frame);
  DCHECK(!call_frame->hasHostCallFrameFlag());

  // Get the caller.
  intptr_t source_id;
  int line_number;
  String url;
  JSC::JSValue function;
  call_frame->interpreter()->retrieveLastCaller(call_frame, line_number,
                                                source_id, url, function);
  call_frame = call_frame->callerFrame();

  if (!call_frame || call_frame->hasHostCallFrameFlag()) {
    // No valid JavaScript caller, return NULL;
    return scoped_ptr<CallFrame>();
  }

  return scoped_ptr<CallFrame>(new JSCCallFrame(
      JSC::DebuggerCallFrame(call_frame), source_id, line_number));
}

std::string JSCCallFrame::GetFunctionName() {
  return call_frame_.functionName().latin1().data();
}

int JSCCallFrame::GetLineNumber() {
  // Devtools appears to want 0-based line numbers, hence the -1.
  DCHECK_GT(line_number_, 0);
  return line_number_ - 1;
}

base::optional<int> JSCCallFrame::GetColumnNumber() { return column_number_; }

std::string JSCCallFrame::GetScriptId() { return script_id_; }

scoped_ptr<Scope> JSCCallFrame::GetScopeChain() {
  DCHECK(call_frame_.callFrame());
  DCHECK(call_frame_.callFrame()->scope());
  return scoped_ptr<Scope>(new JSCScope(call_frame_.callFrame()->scope()));
}

const OpaqueHandleHolder* JSCCallFrame::GetThis() { return this_holder_.get(); }

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
