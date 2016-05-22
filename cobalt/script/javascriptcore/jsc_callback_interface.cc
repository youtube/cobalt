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

#include "cobalt/script/javascriptcore/jsc_callback_interface.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSScope.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// Helper class to get the actual callable object from a JSObject.
JSC::CallType GetCallableForCallbackInterface(
    JSC::ExecState* exec_state, JSC::JSObject* implementing_object,
    const char* property_name, JSC::JSValue* out_callable,
    JSC::CallData* out_call_data) {
  DCHECK(implementing_object);
  DCHECK(out_callable);
  DCHECK(out_call_data);

  JSC::JSValue callable = implementing_object;
  JSC::CallType call_type = JSC::getCallData(callable, *out_call_data);
  if (call_type == JSC::CallTypeNone) {
    JSC::Identifier identifier(exec_state, property_name);
    callable = implementing_object->get(exec_state, identifier);
    call_type = JSC::getCallData(callable, *out_call_data);
  }
  if (call_type != JSC::CallTypeNone) {
    *out_callable = callable;
  }
  return call_type;
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
