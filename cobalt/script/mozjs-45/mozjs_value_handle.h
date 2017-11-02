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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_VALUE_HANDLE_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_VALUE_HANDLE_H_

#include "base/optional.h"
#include "cobalt/script/mozjs-45/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs-45/type_traits.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "cobalt/script/value_handle.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// A wrapper around a JS::Value that can be passed into Cobalt as an script
// value object.
//
// An ValueHandle is never passed into Cobalt as-is, but only when wrapped as a
// ScriptValue<ValueHandle>.
class MozjsValueHandle : public ValueHandle {
 public:
  typedef ValueHandle BaseType;
  JSObject* handle() const { return handle_.GetObject(); }
  const JS::Value& value() const { return handle_.GetValue(); }
  bool WasCollected() const { return handle_.WasCollected(); }

 private:
  MozjsValueHandle(JSContext* context, JS::HandleValue value)
      : handle_(context, value) {}
  ~MozjsValueHandle() {}

  WeakHeapObject handle_;

  friend class MozjsUserObjectHolder<MozjsValueHandle>;
  friend class base::optional<MozjsValueHandle>;
};

typedef MozjsUserObjectHolder<MozjsValueHandle> MozjsValueHandleHolder;

template <>
struct TypeTraits<ValueHandle> {
  typedef MozjsValueHandleHolder ConversionType;
  typedef const ScriptValue<ValueHandle>* ReturnType;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_VALUE_HANDLE_H_
