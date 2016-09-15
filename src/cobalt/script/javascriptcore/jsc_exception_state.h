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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_EXCEPTION_STATE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_EXCEPTION_STATE_H_

#include "base/threading/thread_checker.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCExceptionState : public ExceptionState {
 public:
  explicit JSCExceptionState(JSCGlobalObject* global_object)
      : global_object_(global_object), exception_(NULL) {}
  // ExceptionState interface
  void SetException(const scoped_refptr<ScriptException>& exception) OVERRIDE;
  void SetSimpleException(MessageType message_type, ...) OVERRIDE;

  bool is_exception_set() const { return (exception_ != NULL); }
  JSC::JSObject* exception_object() { return exception_; }

 private:
  JSCGlobalObject* global_object_;
  JSC::JSObject* exception_;
  base::ThreadChecker thread_checker_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_EXCEPTION_STATE_H_
