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

#include "cobalt/script/javascriptcore/jsc_scope.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSScope.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"

namespace cobalt {
namespace script {

namespace javascriptcore {

JSCScope::JSCScope(JSC::JSScope* scope) : scope_(scope), global_object_(NULL) {
  DCHECK(scope_);
  global_object_ = JSC::jsCast<JSCGlobalObject*>(scope->globalObject());
  DCHECK(global_object_);

  scope_holder_.reset(new JSCObjectHandleHolder(
      JSCObjectHandle(scope_), global_object_->script_object_registry()));
  DCHECK(scope_holder_);
}

JSCScope::~JSCScope() {}

const OpaqueHandleHolder* JSCScope::GetObject() { return scope_holder_.get(); }

Scope::Type JSCScope::GetType() {
  DCHECK(scope_);
  int type = scope_->structure()->typeInfo().type();
  std::string type_string;
  switch (type) {
    case JSC::GlobalObjectType:
      return kGlobal;
    case JSC::ActivationObjectType:
      return kLocal;
    default: {
      DLOG(WARNING) << "Unexpected scope type: " << type;
      return kLocal;
    }
  }
}

scoped_ptr<Scope> JSCScope::GetNext() {
  DCHECK(scope_);
  if (scope_->next()) {
    return scoped_ptr<Scope>(new JSCScope(scope_->next()));
  } else {
    return scoped_ptr<Scope>();
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
