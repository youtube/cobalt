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

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/jsc_call_frame.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/jsc_global_object_proxy.h"
#include "cobalt/script/javascriptcore/jsc_object_handle_holder.h"
#include "cobalt/script/javascriptcore/jsc_source_provider.h"

#include "third_party/WebKit/Source/JavaScriptCore/debugger/DebuggerCallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/heap/MarkedBlock.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/CallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceProvider.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Executable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSCell.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSFunction.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSScope.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"

namespace cobalt {
namespace script {

// Static factory method declared in public interface.
scoped_ptr<ScriptDebugger> ScriptDebugger::CreateDebugger(
    GlobalObjectProxy* global_object_proxy, Delegate* delegate) {
  return scoped_ptr<ScriptDebugger>(
      new javascriptcore::JSCDebugger(global_object_proxy, delegate));
}

namespace javascriptcore {

namespace {
// Type used to store a set of source providers.
typedef WTF::HashSet<JSC::SourceProvider*> SourceProviderSet;

// Functor to iterate over the JS cells and gather source providers.
class GathererFunctor : public JSC::MarkedBlock::VoidFunctor {
 public:
  GathererFunctor(JSC::JSGlobalObject* global_object,
                  SourceProviderSet* source_providers)
      : global_object_(global_object), source_providers_(source_providers) {}

  void operator()(JSC::JSCell* cell) {
    JSC::JSFunction* function = JSC::jsDynamicCast<JSC::JSFunction*>(cell);
    if (function && !function->isHostFunction() &&
        function->scope()->globalObject() == global_object_ &&
        function->executable()->isFunctionExecutable()) {
      source_providers_->add(
          JSC::jsCast<JSC::FunctionExecutable*>(function->executable())
              ->source()
              .provider());
    }
  }

 private:
  SourceProviderSet* source_providers_;
  JSC::JSGlobalObject* global_object_;
};

// Uses the GatherorFunctor defined above to gather all the currently parsed
// source providers defined in |global_object| and populate |source_providers|.
// This is called once by the |attach| method; the script debugger is
// automatically notified of subsequently parsed scripts via the |source_parsed|
// method.
void GatherSourceProviders(JSC::JSGlobalObject* global_object,
                           SourceProviderSet* source_providers) {
  DCHECK(global_object);
  DCHECK(source_providers);
  source_providers->clear();
  GathererFunctor gatherer_functor(global_object, source_providers);
  JSC::JSGlobalData& global_data = global_object->globalData();
  global_data.heap.objectSpace().forEachLiveCell(gatherer_functor);
}

intptr_t StringToIntptr(const std::string& input) {
  COMPILE_ASSERT(sizeof(int64) >= sizeof(intptr_t),
                 int64_not_big_enough_to_store_intptr_t);
  int64 as_int64 = 0;
  bool did_convert = base::StringToInt64(input, &as_int64);
  DCHECK(did_convert);
  return static_cast<intptr_t>(as_int64);
}
}  // namespace

JSCDebugger::JSCDebugger(GlobalObjectProxy* global_object_proxy,
                         Delegate* delegate)
    : global_object_proxy_(global_object_proxy),
      delegate_(delegate),
      pause_on_exceptions_(kNone),
      pause_on_next_statement_(false),
      pause_on_call_frame_(NULL),
      current_call_frame_(NULL),
      current_source_id_(0),
      current_line_number_(0),
      current_column_number_(0),
      is_paused_(false) {}

JSCDebugger::~JSCDebugger() {
  if (GetGlobalObject()->debugger() == this) {
    detach(GetGlobalObject());
  }
}

void JSCDebugger::Attach() {
  if (GetGlobalObject()->debugger() == NULL) {
    attach(GetGlobalObject());
  } else {
    DLOG(WARNING) << "Debugger is already attached.";
  }
}

void JSCDebugger::Detach() {
  if (GetGlobalObject()->debugger() == this) {
    detach(GetGlobalObject());
  } else {
    DLOG(WARNING) << "Debugger is not attached.";
  }
}

void JSCDebugger::Pause() {
  pause_on_next_statement_ = true;
  pause_on_call_frame_ = NULL;
}

void JSCDebugger::Resume() {
  pause_on_next_statement_ = false;
  pause_on_call_frame_ = NULL;
}

void JSCDebugger::SetBreakpoint(const std::string& script_id, int line_number,
                                int column_number) {
  // Convert the string script_id used by devtools into the intptr_t source_id
  // used internally.
  intptr_t source_id = StringToIntptr(script_id);

  breakpoints_.push_back(Breakpoint(source_id, line_number, column_number));
}

script::ScriptDebugger::PauseOnExceptionsState
JSCDebugger::SetPauseOnExceptions(PauseOnExceptionsState state) {
  const PauseOnExceptionsState previous_state = pause_on_exceptions_;
  pause_on_exceptions_ = state;
  return previous_state;
}

void JSCDebugger::StepInto() {
  pause_on_next_statement_ = true;
  pause_on_call_frame_ = NULL;
}

void JSCDebugger::StepOut() {
  pause_on_next_statement_ = false;
  const JSC::CallFrame* call_frame = current_call_frame_.callFrame();
  pause_on_call_frame_ = call_frame ? call_frame->callerFrame() : NULL;
}

void JSCDebugger::StepOver() {
  pause_on_next_statement_ = false;
  pause_on_call_frame_ = current_call_frame_.callFrame();
  DCHECK(pause_on_call_frame_);
}

JSCGlobalObject* JSCDebugger::GetGlobalObject() const {
  return base::polymorphic_downcast<JSCGlobalObjectProxy*>(global_object_proxy_)
      ->global_object();
}

void JSCDebugger::attach(JSC::JSGlobalObject* global_object) {
  DCHECK(global_object);
  JSC::Debugger::attach(global_object);

  // Gather the source providers and call |sourceParsed| on each one.
  // Any scripts parsed after this point will automatically invoke a call
  // to |sourceParsed|.
  SourceProviderSet source_providers;
  GatherSourceProviders(global_object, &source_providers);

  for (SourceProviderSet::iterator iter = source_providers.begin();
       iter != source_providers.end(); ++iter) {
    sourceParsed(global_object->globalExec(), *iter, -1, String());
  }
}

void JSCDebugger::detach(JSC::JSGlobalObject* global_object) {
  DCHECK(global_object);
  JSC::Debugger::detach(global_object);
  delegate_->OnScriptDebuggerDetach("canceled_by_user");
}

void JSCDebugger::sourceParsed(JSC::ExecState* exec_state,
                               JSC::SourceProvider* source_provider,
                               int error_line,
                               const WTF::String& error_message) {
  UNREFERENCED_PARAMETER(exec_state);
  DCHECK(source_provider);

  if (error_line < 0) {
    // Script was parsed successfully.
    delegate_->OnScriptParsed(
        scoped_ptr<SourceProvider>(new JSCSourceProvider(source_provider)));
  } else {
    // Script failed to parse.
    delegate_->OnScriptFailedToParse(
        scoped_ptr<SourceProvider>(new JSCSourceProvider(
            source_provider, error_line, error_message.utf8().data())));
  }
}

void JSCDebugger::exception(const JSC::DebuggerCallFrame& call_frame,
                            intptr_t source_id, int line_number,
                            int column_number, bool has_handler) {
  if (pause_on_exceptions_ == kAll ||
      (pause_on_exceptions_ == kUncaught && !has_handler)) {
    pause_on_next_statement_ = true;
    pause_on_call_frame_ = NULL;
  }

  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::atStatement(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number) {
  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::callEvent(const JSC::DebuggerCallFrame& call_frame,
                            intptr_t source_id, int line_number,
                            int column_number) {
  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::returnEvent(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number) {
  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::willExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                                     intptr_t source_id, int line_number,
                                     int column_number) {
  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::didExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                                    intptr_t source_id, int line_number,
                                    int column_number) {
  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::didReachBreakpoint(const JSC::DebuggerCallFrame& call_frame,
                                     intptr_t source_id, int line_number,
                                     int column_number) {
  pause_on_next_statement_ = true;
  pause_on_call_frame_ = NULL;
  UpdateAndPauseIfNeeded(call_frame, source_id, line_number, column_number);
}

void JSCDebugger::UpdateAndPauseIfNeeded(
    const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
    int line_number, int column_number) {
  // Don't do anything if we're currently paused. We want to remember the call
  // frame and source location at the point we paused, not override them with
  // any debugging scripts that get evaluated while paused.
  if (is_paused_) {
    return;
  }

  UpdateSourceLocation(source_id, line_number, column_number);
  UpdateCallFrame(call_frame);
  PauseIfNeeded(call_frame);
}

void JSCDebugger::UpdateSourceLocation(intptr_t source_id, int line_number,
                                       int column_number) {
  current_source_id_ = source_id;
  current_line_number_ = line_number;
  current_column_number_ = column_number;
}

void JSCDebugger::UpdateCallFrame(const JSC::DebuggerCallFrame& call_frame) {
  current_call_frame_ = call_frame;
}

void JSCDebugger::PauseIfNeeded(const JSC::DebuggerCallFrame& call_frame) {
  // Determine whether we should pause.
  bool will_pause = pause_on_next_statement_;
  will_pause |=
      pause_on_call_frame_ && pause_on_call_frame_ == call_frame.callFrame();
  will_pause |= IsBreakpointAtCurrentLocation();

  if (!will_pause) {
    return;
  }

  // Set the |is_paused_| state for the remainder of this function.
  ScopedPausedState paused(this);

  // Delegate handles the actual blocking of the thread to implement Pause.
  delegate_->OnScriptDebuggerPause(scoped_ptr<CallFrame>(
      new JSCCallFrame(call_frame, current_source_id_, current_line_number_,
                       current_column_number_)));
}

bool JSCDebugger::IsBreakpointAtCurrentLocation() const {
  for (BreakpointVector::const_iterator it = breakpoints_.begin();
       it != breakpoints_.end(); ++it) {
    if (it->source_id == current_source_id_ &&
        it->line_number == current_line_number_ &&
        (it->column_number == 0 ||
         it->column_number == current_column_number_)) {
      return true;
    }
  }
  return false;
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
