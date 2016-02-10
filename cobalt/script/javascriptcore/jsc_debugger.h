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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/javascript_debugger_interface.h"
#include "cobalt/script/javascriptcore/jsc_global_object_proxy.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/debugger/Debugger.h"
#include "third_party/WebKit/Source/WTF/wtf/HashSet.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace JSC {
class DebuggerCallFrame;
class ExecState;
class JSGlobalData;
class JSGlobalObject;
class JSValue;
class SourceProvider;
}

namespace cobalt {
namespace script {
namespace javascriptcore {

// JavaScriptCore-specific implementation of a JavaScript debugger.
// Uses multiple inheritance in accordance with the C++ style guide to extend
// JSC::Debugger and implement JavaScriptDebuggerInterface.
// https://engdoc.***REMOVED***/eng/doc/devguide/cpp/styleguide.shtml?cl=head#Multiple_Inheritance
// Only the JavaScriptDebuggerInterface is publicly exposed.
// This class is not designed to be thread-safe - it is assumed that all
// public methods will be run on the same message loop as the JavaScript
// global object to which this debugger connects.
class JSCDebugger : protected JSC::Debugger,
                    public JavaScriptDebuggerInterface {
 public:
  JSCDebugger(GlobalObjectProxy* global_object_proxy,
              const OnEventCallback& on_event_callback,
              const OnDetachCallback& on_detach_callback);

  ~JSCDebugger() OVERRIDE;

  // Implementation of JavaScriptDebuggerInterface.
  scoped_ptr<base::DictionaryValue> GetScriptSource(
      const scoped_ptr<base::DictionaryValue>& params) OVERRIDE;

 protected:
  // Hides a non-virtual JSC::Debugger method with the same name.
  void attach(JSC::JSGlobalObject* global_object);

  // The following methods are overrides of pure virtual methods in
  // JSC::Debugger, hence the non-standard names.
  void detach(JSC::JSGlobalObject* global_object) OVERRIDE;

  void sourceParsed(JSC::ExecState* exec_state,
                    JSC::SourceProvider* source_provider, int error_line,
                    const WTF::String& error_message) OVERRIDE;

  void exception(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                 int line_number, int column_number, bool has_handler) OVERRIDE;

  void atStatement(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                   int line_number, int column_number) OVERRIDE;

  void callEvent(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                 int line_number, int column_number) OVERRIDE;

  void returnEvent(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                   int line_number, int column_number) OVERRIDE;

  void willExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                          intptr_t source_id, int line_number,
                          int column_number) OVERRIDE;

  void didExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                         intptr_t source_id, int line_number,
                         int column_number) OVERRIDE;

  void didReachBreakpoint(const JSC::DebuggerCallFrame& call_frame,
                          intptr_t source_id, int line_number,
                          int column_number) OVERRIDE;

 private:
  // Type used to store a set of source providers.
  typedef WTF::HashSet<JSC::SourceProvider*> SourceProviderSet;

  // Functor to iterate over the JS cells and gather source providers.
  struct GathererFunctor : public JSC::MarkedBlock::VoidFunctor {
    GathererFunctor(JSC::JSGlobalObject* global_object,
                    SourceProviderSet* source_providers)
        : global_object_(global_object), source_providers_(source_providers) {}

    void operator()(JSC::JSCell* cell);

    SourceProviderSet* source_providers_;
    JSC::JSGlobalObject* global_object_;
  };

  // Convenience function to get the global object pointer from the proxy.
  JSC::JSGlobalObject* GetGlobalObject() const;

  // Populates the set of source providers.
  void GatherSourceProviders(JSC::JSGlobalObject* global_object);

  base::ThreadChecker thread_checker_;
  GlobalObjectProxy* global_object_proxy_;

  // Notification callbacks.
  OnEventCallback on_event_callback_;
  OnDetachCallback on_detach_callback_;

  // Set of source providers (scripts).
  SourceProviderSet source_providers_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_
