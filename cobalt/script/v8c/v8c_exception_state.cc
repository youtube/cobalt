// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/v8c/v8c_exception_state.h"

namespace cobalt {
namespace script {
namespace v8c {
namespace {

v8::Local<v8::Value> ToV8cException(v8::Isolate* isolate,
                                    SimpleExceptionType type,
                                    const std::string& error_message) {
  v8::Local<v8::Value> error;
  v8::Local<v8::String> message =
      v8::String::NewFromUtf8(isolate, error_message.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked();
  switch (type) {
    case kError:
      error = v8::Exception::Error(message, isolate);
      break;
    case kTypeError:
      error = v8::Exception::TypeError(message, isolate);
      break;
    case kRangeError:
      error = v8::Exception::RangeError(message, isolate);
      break;
    case kReferenceError:
      error = v8::Exception::ReferenceError(message, isolate);
      break;
    case kSyntaxError:
      error = v8::Exception::SyntaxError(message, isolate);
      break;
    case kURIError:
      // TODO: Don't see anything for this...
      error = v8::Exception::Error(message, isolate);
      break;
    default:
      NOTREACHED();
      error = v8::Exception::Error(message, isolate);
  }

  return error;
}

}  // namespace

// TODO: Consider just exposing |ToV8cException| instead.
v8::Local<v8::Value> CreateErrorObject(v8::Isolate* isolate,
                                       SimpleExceptionType type) {
  return ToV8cException(isolate, type, "");
}

void V8cExceptionState::SetException(
    const scoped_refptr<ScriptException>& exception) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_exception_set_);

  V8cGlobalEnvironment* global_environment =
      V8cGlobalEnvironment::GetFromIsolate(isolate_);
  if (!global_environment) return;
  v8::Local<v8::Object> wrapper =
      global_environment->wrapper_factory()->GetWrapper(exception);

  isolate_->ThrowException(wrapper);
  is_exception_set_ = true;
}

void V8cExceptionState::SetSimpleExceptionVA(SimpleExceptionType type,
                                             const char* format,
                                             va_list& arguments) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_exception_set_);

  std::string error_message = base::StringPrintV(format, arguments);
  v8::Local<v8::Value> error = ToV8cException(isolate_, type, error_message);
  isolate_->ThrowException(error);
  is_exception_set_ = true;
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
