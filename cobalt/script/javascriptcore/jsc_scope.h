/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SCOPE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SCOPE_H_

#include <string>

#include "base/optional.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/jsc_object_handle_holder.h"
#include "cobalt/script/scope.h"

namespace JSC {
class JSScope;
}

namespace cobalt {
namespace script {
namespace javascriptcore {

// JavaScriptCore-specific implementation of a JavaScript call frame.
class JSCScope : public Scope {
 public:
  explicit JSCScope(JSC::JSScope* scope);
  ~JSCScope() OVERRIDE;

  const OpaqueHandleHolder* GetObject() OVERRIDE;
  Type GetType() OVERRIDE;
  scoped_ptr<Scope> GetNext() OVERRIDE;

 private:
  JSC::JSScope* scope_;
  JSCGlobalObject* global_object_;
  scoped_ptr<JSCObjectHandleHolder> scope_holder_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SCOPE_H_
