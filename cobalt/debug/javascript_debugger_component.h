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
#ifndef COBALT_DEBUG_JAVASCRIPT_DEBUGGER_COMPONENT_H_
#define COBALT_DEBUG_JAVASCRIPT_DEBUGGER_COMPONENT_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/script/call_frame.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/scope.h"
#include "cobalt/script/script_debugger.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/source_provider.h"

namespace cobalt {
namespace debug {

class JavaScriptDebuggerComponent : public DebugServer::Component,
                                    public script::ScriptDebugger::Delegate {
 public:
  JavaScriptDebuggerComponent(const base::WeakPtr<DebugServer>& server,
                              script::GlobalObjectProxy* global_object_proxy);

  ~JavaScriptDebuggerComponent() OVERRIDE;

 private:
  // Type to store a map of SourceProvider pointers, keyed by string ID.
  typedef std::map<std::string, script::SourceProvider*> SourceProviderMap;

  JSONObject Enable(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);

  // Gets the source of a specified script.
  JSONObject GetScriptSource(const JSONObject& params);

  // Code execution control commands.
  JSONObject Pause(const JSONObject& params);
  JSONObject Resume(const JSONObject& params);
  JSONObject StepInto(const JSONObject& params);
  JSONObject StepOut(const JSONObject& params);
  JSONObject StepOver(const JSONObject& params);

  // ScriptDebugger::Delegate implementation.
  void OnScriptDebuggerDetach(const std::string& reason) OVERRIDE;
  void OnScriptDebuggerPause(scoped_ptr<script::CallFrame> call_frame) OVERRIDE;
  void OnScriptFailedToParse(
      scoped_ptr<script::SourceProvider> source_provider) OVERRIDE;
  void OnScriptParsed(
      scoped_ptr<script::SourceProvider> source_provider) OVERRIDE;

  // Creates a JSON object describing a single call frame.
  JSONObject CreateCallFrameData(
      const scoped_ptr<script::CallFrame>& call_frame);

  // Creates an array of JSON objects describing the call stack starting from
  // the specified call frame. Takes ownership of |call_frame|.
  JSONList CreateCallStackData(scoped_ptr<script::CallFrame> call_frame);

  // Creates a JSON object describing a single scope object.
  JSONObject CreateScopeData(const scoped_ptr<script::Scope>& scope);

  // Creates an array of JSON objects describing the scope chain starting from
  // the specified scope object. Takes ownership of |scope|.
  JSONList CreateScopeChainData(scoped_ptr<script::Scope> scope);

  // Called by |OnScriptFailedToParse| and |OnScriptParsed|.
  // Stores the source provider in |source_providers_| and dispatches the
  // specified event notification to the clients.
  void HandleScriptEvent(const std::string& method,
                         scoped_ptr<script::SourceProvider> source_provider);

  // Sends a Debugger.paused event to the clients with call stack data.
  void SendPausedEvent(scoped_ptr<script::CallFrame> call_frame);

  // Sends a Debugger.resumed event to the clients with no parameters.
  void SendResumedEvent();

  // No ownership.
  script::GlobalObjectProxy* global_object_proxy_;

  // Handles all debugging interaction with the JavaScript engine.
  scoped_ptr<script::ScriptDebugger> script_debugger_;

  // Map of source providers with scoped deleter to clean up on destruction.
  SourceProviderMap source_providers_;
  STLValueDeleter<SourceProviderMap> source_providers_deleter_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_JAVASCRIPT_DEBUGGER_COMPONENT_H_
