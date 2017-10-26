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
#include "cobalt/script/v8c/native_promise.h"
#include "cobalt/script/v8c/v8c_global_environment.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cScriptValueFactory : public ScriptValueFactory {
 public:
  template <typename T>
  scoped_ptr<ScriptValue<Promise<T>>> CreatePromise() {
    typedef ScriptValue<Promise<T>> ScriptPromiseType;
    typedef V8cUserObjectHolder<NativePromise<T>> V8cPromiseHolderType;
    NOTIMPLEMENTED();
    return make_scoped_ptr<ScriptValue<Promise<T>>>(nullptr);
  }
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_SCRIPT_VALUE_FACTORY_H_
