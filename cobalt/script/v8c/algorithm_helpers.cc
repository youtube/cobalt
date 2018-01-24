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

#include "cobalt/script/v8c/algorithm_helpers.h"

#include "base/logging.h"
#include "cobalt/script/v8c/helpers.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/v8c_global_environment.h"

namespace cobalt {
namespace script {
namespace v8c {

v8::MaybeLocal<v8::Object> GetIterator(v8::Isolate* isolate,
                                       v8::Local<v8::Object> object,
                                       V8cExceptionState* exception_state) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> iterator_getter;
  if (!object->Get(context, v8::Symbol::GetIterator(isolate))
           .ToLocal(&iterator_getter)) {
    exception_state->ReThrow(&try_catch);
    return {};
  }
  if (!iterator_getter->IsFunction()) {
    exception_state->SetSimpleException(kTypeError,
                                        "Iterator getter is not callable.");
    return {};
  }
  v8::Local<v8::Value> iterator;
  if (!MaybeCallAsFunction(context, iterator_getter, object)
           .ToLocal(&iterator)) {
    exception_state->ReThrow(&try_catch);
    return {};
  }
  if (!iterator->IsObject()) {
    exception_state->SetSimpleException(kTypeError,
                                        "Iterator is not an object.");
    return {};
  }
  return iterator.As<v8::Object>();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
