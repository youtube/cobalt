// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_OPAQUE_HANDLE_H_
#define COBALT_SCRIPT_OPAQUE_HANDLE_H_

#include "cobalt/script/script_value.h"

namespace cobalt {
namespace script {

// A handle to an object that is managed by the JavaScript Engine. There should
// be no need for Cobalt code to operate on this handle, aside from managing its
// lifetime.
class OpaqueHandle {
 protected:
  OpaqueHandle() {}
  virtual ~OpaqueHandle() {}
};

typedef ScriptValue<OpaqueHandle> OpaqueHandleHolder;

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_OPAQUE_HANDLE_H_
