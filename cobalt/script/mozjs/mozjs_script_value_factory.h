/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_MOZJS_SCRIPT_VALUE_FACTORY_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_SCRIPT_VALUE_FACTORY_H_

#include "cobalt/script/mozjs/mozjs_global_environment.h"
#include "cobalt/script/mozjs/native_promise.h"
#include "cobalt/script/mozjs/promise_wrapper.h"
#include "cobalt/script/script_value_factory.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
class MozjsScriptValueFactory : public ScriptValueFactory {
 public:
  explicit MozjsScriptValueFactory(MozjsGlobalEnvironment* global_environment);
  ~MozjsScriptValueFactory() OVERRIDE {}

  template <typename T>
  scoped_ptr<ScriptValue<Promise<T> > > CreatePromise() {
    typedef ScriptValue<Promise<T> > ScriptPromiseType;
    typedef MozjsUserObjectHolder<NativePromise<T> > MozjsPromiseHolderType;

    JSContext* context = global_environment_->context();
    JS::RootedObject global_object(context,
                                   global_environment_->global_object());
    JSAutoRequest auto_request(context);
    JSAutoCompartment auto_compartment(context, global_object);

    JS::RootedObject promise_wrapper(
        context, PromiseWrapper::Create(context, global_object));
    DCHECK(promise_wrapper);
    scoped_ptr<ScriptPromiseType> promise(new MozjsPromiseHolderType(
        promise_wrapper, context, global_environment_->wrapper_factory()));
    return promise.Pass();
  }

 private:
  MozjsGlobalEnvironment* global_environment_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_MOZJS_MOZJS_SCRIPT_VALUE_FACTORY_H_
