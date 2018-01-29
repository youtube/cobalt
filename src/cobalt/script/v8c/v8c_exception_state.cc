// Copyright 2017 Google Inc. All Rights Reserved.
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
      error = v8::Exception::Error(message);
      break;
    case kTypeError:
      error = v8::Exception::TypeError(message);
      break;
    case kRangeError:
      error = v8::Exception::RangeError(message);
      break;
    case kReferenceError:
      error = v8::Exception::ReferenceError(message);
      break;
    case kSyntaxError:
      error = v8::Exception::SyntaxError(message);
      break;
    case kURIError:
      // TODO: Don't see anything for this...
      error = v8::Exception::Error(message);
      break;
    default:
      NOTREACHED();
      error = v8::Exception::Error(message);
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
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_exception_set_);

  V8cGlobalEnvironment* global_environment =
      V8cGlobalEnvironment::GetFromIsolate(isolate_);
  v8::Local<v8::Object> wrapper =
      global_environment->wrapper_factory()->GetWrapper(exception);

  isolate_->ThrowException(wrapper);
  is_exception_set_ = true;
}

void V8cExceptionState::SetSimpleExceptionVA(SimpleExceptionType type,
                                             const char* format,
                                             va_list arguments) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_exception_set_);

  std::string error_message = base::StringPrintV(format, arguments);
  v8::Local<v8::Value> error = ToV8cException(isolate_, type, error_message);
  isolate_->ThrowException(error);
  is_exception_set_ = true;
}

void V8cExceptionState::ReThrow(v8::TryCatch* try_catch) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_exception_set_);
  DCHECK(try_catch);
  DCHECK(try_catch->HasCaught());

  is_exception_set_ = true;
  try_catch->ReThrow();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
