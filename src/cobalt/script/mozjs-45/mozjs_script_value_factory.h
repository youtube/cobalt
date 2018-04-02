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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_SCRIPT_VALUE_FACTORY_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_SCRIPT_VALUE_FACTORY_H_

#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "cobalt/script/mozjs-45/native_promise.h"
#include "cobalt/script/mozjs-45/promise_wrapper.h"
#include "cobalt/script/script_value_factory.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsScriptValueFactory : public ScriptValueFactory {
 public:
  explicit MozjsScriptValueFactory(MozjsGlobalEnvironment* global_environment);

  template <typename T>
  Handle<Promise<T>> CreatePromise() {
    using MozjsPromiseHolderType = MozjsUserObjectHolder<NativePromise<T>>;

    JSContext* context = global_environment_->context();
    JS::RootedObject global_object(context,
                                   global_environment_->global_object());
    JSAutoRequest auto_request(context);
    JSAutoCompartment auto_compartment(context, global_object);

    JS::RootedValue promise_wrapper(context);
    promise_wrapper.setObjectOrNull(PromiseWrapper::Create(context));
    return Handle<Promise<T>>(
        new MozjsPromiseHolderType(context, promise_wrapper));
  }

 private:
  MozjsGlobalEnvironment* global_environment_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_SCRIPT_VALUE_FACTORY_H_
