// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/v8c/v8c_script_debugger.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "v8/include/v8-inspector.h"

namespace cobalt {
namespace script {
namespace v8c {

namespace {
constexpr int kContextGroupId = 1;
constexpr char kContextName[] = "Cobalt";
}  // namespace

V8cScriptDebugger::V8cScriptDebugger(
    V8cGlobalEnvironment* v8c_global_environment, Delegate* delegate)
    : global_environment_(v8c_global_environment),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          inspector_(v8_inspector::V8Inspector::create(
              global_environment_->isolate(), /* client */ this))),
      // Immediately connect a single long-running inspector session.
      ALLOW_THIS_IN_INITIALIZER_LIST(inspector_session_(inspector_->connect(
          kContextGroupId, this, v8_inspector::StringView()))),
      pause_on_exception_state_(kAll) {
  // Register our one-and-only context with the inspector.
  v8::Isolate* isolate = global_environment_->isolate();
  EntryScope entry_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  inspector_->contextCreated(v8_inspector::V8ContextInfo(
      context, kContextGroupId,
      v8_inspector::StringView(reinterpret_cast<const uint8_t*>(kContextName),
                               sizeof(kContextName) - 1)));
}

V8cScriptDebugger::~V8cScriptDebugger() {
  v8::Isolate* isolate = global_environment_->isolate();
  EntryScope entry_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  inspector_->contextDestroyed(context);
}

ScriptDebugger::PauseOnExceptionsState V8cScriptDebugger::SetPauseOnExceptions(
    ScriptDebugger::PauseOnExceptionsState state) {
  DCHECK(inspector_session_);
  auto previous_state = pause_on_exception_state_;
  pause_on_exception_state_ = state;
  inspector_session_->setSkipAllPauses(state == kNone);
  return previous_state;
}

bool V8cScriptDebugger::CanDispatchProtocolMethod(const std::string& method) {
  DCHECK(inspector_session_);
  return inspector_session_->canDispatchMethod(v8_inspector::StringView(
      reinterpret_cast<const uint8_t*>(method.c_str()), method.length()));
}

void V8cScriptDebugger::DispatchProtocolMessage(const std::string& message) {
  DCHECK(inspector_session_);
  inspector_session_->dispatchProtocolMessage(v8_inspector::StringView(
      reinterpret_cast<const uint8_t*>(message.c_str()), message.length()));
}

// v8_inspector::V8InspectorClient implementation.
void V8cScriptDebugger::runMessageLoopOnPause(int contextGroupId) {
  DCHECK(contextGroupId == kContextGroupId);
  if (attached_) {
    delegate_->OnScriptDebuggerPause();
  }
}

// v8_inspector::V8InspectorClient implementation.
void V8cScriptDebugger::quitMessageLoopOnPause() {
  if (attached_) {
    delegate_->OnScriptDebuggerResume();
  }
}

// v8_inspector::V8InspectorClient implementation.
void V8cScriptDebugger::consoleAPIMessage(
    int contextGroupId, v8::Isolate::MessageErrorLevel level,
    const v8_inspector::StringView& message,
    const v8_inspector::StringView& url, unsigned lineNumber,
    unsigned columnNumber, v8_inspector::V8StackTrace* trace) {
  UNREFERENCED_PARAMETER(contextGroupId);
  UNREFERENCED_PARAMETER(trace);

  std::stringstream log;
  if (url.length()) {
    log << '[' << FromStringView(url) << ", Line " << lineNumber << ", Col "
        << columnNumber << ']';
  }
  log << FromStringView(message);

  switch (level) {
    case v8::Isolate::kMessageError:
      LOG(ERROR) << log.str();
      break;
    case v8::Isolate::kMessageWarning:
      LOG(WARNING) << log.str();
      break;
    default:
      LOG(INFO) << log.str();
  }
}

// v8_inspector::V8InspectorClient implementation.
v8::Local<v8::Context> V8cScriptDebugger::ensureDefaultContextInGroup(
    int contextGroupId) {
  v8::Isolate* isolate = global_environment_->isolate();
  EntryScope entry_scope(isolate);
  if (contextGroupId == kContextGroupId) {
    return isolate->GetCurrentContext();
  }
  DLOG(WARNING) << "No default context for group " << contextGroupId;
  return v8::Local<v8::Context>();
}

// v8_inspector::V8Inspector::Channel implementation.
void V8cScriptDebugger::sendResponse(
    int callId, std::unique_ptr<v8_inspector::StringBuffer> message) {
  if (attached_) {
    std::string response = FromStringView(message->string());
    delegate_->OnScriptDebuggerResponse(response);
  }
}

// v8_inspector::V8Inspector::Channel implementation.
void V8cScriptDebugger::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  if (attached_) {
    std::string event = FromStringView(message->string());
    delegate_->OnScriptDebuggerEvent(event);
  }
}

// Inspired by |CopyCharsUnsigned| in v8/src/utils.h
std::string V8cScriptDebugger::FromStringView(
    const v8_inspector::StringView& string_view) {
  std::string string;
  if (string_view.is8Bit()) {
    string.assign(reinterpret_cast<const char*>(string_view.characters8()),
                  string_view.length());
  } else {
    string.reserve(string_view.length());
    const uint16_t* chars =
        reinterpret_cast<const uint16_t*>(string_view.characters16());
    for (int i = 0; i < string_view.length(); i++) {
      string += chars[i];
    }
  }
  return string;
}

}  // namespace v8c

// Static factory method declared in public interface.
scoped_ptr<ScriptDebugger> ScriptDebugger::CreateDebugger(
    GlobalEnvironment* global_environment, Delegate* delegate) {
  auto* v8c_global_environment =
      base::polymorphic_downcast<v8c::V8cGlobalEnvironment*>(
          global_environment);
  return scoped_ptr<ScriptDebugger>(
      new v8c::V8cScriptDebugger(v8c_global_environment, delegate));
}

}  // namespace script
}  // namespace cobalt
