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
#include "cobalt/script/javascriptcore/jsc_exception_state.h"

#include <string>

#include "cobalt/script/javascriptcore/conversion_helpers.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Error.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/ErrorInstance.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

void JSCExceptionState::SetException(
    const scoped_refptr<ScriptException>& exception) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(&global_object_->globalData());
  exception_ =
      global_object_->wrapper_factory()->GetWrapper(global_object_, exception);
  DCHECK(exception_->isErrorInstance());
}

void JSCExceptionState::SetSimpleException(MessageType message_type, ...) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(&global_object_->globalData());

  va_list arguments;
  va_start(arguments, message_type);
  WTF::String error_string = ToWTFString(
      base::StringPrintV(GetExceptionMessageFormat(message_type), arguments));
  va_end(arguments);

  switch (GetSimpleExceptionType(message_type)) {
    case kError:
      exception_ = JSC::createError(global_object_, error_string);
      break;
    case kTypeError:
      exception_ = JSC::createTypeError(global_object_, error_string);
      break;
    case kRangeError:
      exception_ = JSC::createRangeError(global_object_, error_string);
      break;
    case kReferenceError:
      exception_ = JSC::createReferenceError(global_object_, error_string);
      break;
    case kSyntaxError:
      exception_ = JSC::createSyntaxError(global_object_, error_string);
      break;
    case kURIError:
      exception_ = JSC::createURIError(global_object_, error_string);
      break;
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
