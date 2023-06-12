// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_SCRIPT_CALLBACK_WRAPPER_H_
#define COBALT_H5VCC_SCRIPT_CALLBACK_WRAPPER_H_

#include <map>
#include <memory>
#include <string>

#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"


namespace cobalt {
namespace h5vcc {

// Template boilerplate for wrapping a script callback safely.
template <typename T>
class ScriptCallbackWrapper
    : public base::RefCountedThreadSafe<ScriptCallbackWrapper<T> > {
 public:
  typedef script::ScriptValue<T> ScriptValue;
  ScriptCallbackWrapper(script::Wrappable* wrappable, const ScriptValue& func)
      : callback(wrappable, func) {}
  typename ScriptValue::Reference callback;
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_SCRIPT_CALLBACK_WRAPPER_H_
