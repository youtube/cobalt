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

#ifndef COBALT_SCRIPT_V8C_V8C_SCRIPT_VALUE_FACTORY_H_
#define COBALT_SCRIPT_V8C_V8C_SCRIPT_VALUE_FACTORY_H_

#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/native_promise.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cScriptValueFactory : public ScriptValueFactory {
 public:
  explicit V8cScriptValueFactory(v8::Isolate* isolate) : isolate_(isolate) {}

  template <typename T>
  scoped_ptr<ScriptValue<Promise<T>>> CreatePromise() {
    typedef ScriptValue<Promise<T>> ScriptPromiseType;
    typedef V8cUserObjectHolder<NativePromise<T>> V8cPromiseHolderType;

    EntryScope entry_scope(isolate_);
    v8::Local<v8::Context> context = isolate_->GetCurrentContext();

    v8::MaybeLocal<v8::Promise::Resolver> maybe_resolver =
        v8::Promise::Resolver::New(context);
    v8::Local<v8::Promise::Resolver> resolver;
    if (!maybe_resolver.ToLocal(&resolver)) {
      return make_scoped_ptr<ScriptPromiseType>(nullptr);
    }

    return make_scoped_ptr<ScriptPromiseType>(
        new V8cPromiseHolderType(isolate_, resolver));
  }

 private:
  v8::Isolate* isolate_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_SCRIPT_VALUE_FACTORY_H_
