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

#include "cobalt/script/mozjs/mozjs_script_value_factory.h"

#include "cobalt/script/mozjs/native_promise.h"

namespace cobalt {
namespace script {
namespace mozjs {

MozjsScriptValueFactory::MozjsScriptValueFactory(
    MozjsGlobalEnvironment* global_environment)
    : global_environment_(global_environment) {}

scoped_ptr<ScriptValueFactory::ScriptPromise>
MozjsScriptValueFactory::CreatePromise() {
  JSContext* context = global_environment_->context();
  JS::RootedObject global_object(context, global_environment_->global_object());
  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_compartment(context, global_object);

  JS::RootedObject native_promise(
      context, NativePromise::Create(context, global_object));
  scoped_ptr<ScriptValueFactory::ScriptPromise> promise;
  promise.reset(new MozjsPromiseHolder(native_promise, context,
                                       global_environment_->wrapper_factory()));
  return promise.Pass();
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
