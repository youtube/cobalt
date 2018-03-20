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

#include "cobalt/script/mozjs-45/mozjs_script_value_factory.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {
namespace mozjs {

MozjsScriptValueFactory::MozjsScriptValueFactory(
    MozjsGlobalEnvironment* global_environment)
    : global_environment_(global_environment) {}

}  // namespace mozjs

// Implementation of template function declared in the base class.
template <typename T>
Handle<Promise<T>> ScriptValueFactory::CreatePromise() {
  mozjs::MozjsScriptValueFactory* mozjs_this =
      base::polymorphic_downcast<mozjs::MozjsScriptValueFactory*>(this);
  return mozjs_this->CreatePromise<T>();
}

}  // namespace script
}  // namespace cobalt

// Explicit template instantiations must go after the template function
// implementation.
#include "cobalt/script/script_value_factory_instantiations.h"
