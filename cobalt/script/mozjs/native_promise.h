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

#ifndef COBALT_SCRIPT_MOZJS_NATIVE_PROMISE_H_
#define COBALT_SCRIPT_MOZJS_NATIVE_PROMISE_H_

#include "cobalt/script/mozjs/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs/type_traits.h"
#include "cobalt/script/mozjs/weak_heap_object.h"
#include "cobalt/script/promise.h"

namespace cobalt {
namespace script {
namespace mozjs {
// SpiderMonkey implementation of the Promise interface.
class NativePromise : public Promise {
 public:
  static JSObject* Create(JSContext* context, JS::HandleObject global_object);

  void Reject() const OVERRIDE;
  void Resolve() const OVERRIDE;

  // ScriptObject boilerplate.
  typedef Promise BaseType;
  JSObject* handle() const { return weak_native_promise_.GetObject(); }
  const JS::Value& value() const { return weak_native_promise_.GetValue(); }
  bool WasCollected() const { return weak_native_promise_.WasCollected(); }

 private:
  NativePromise(JSContext* context, JS::HandleObject object);
  NativePromise(JSContext* context, JS::HandleValue object);

  JSContext* context_;
  WeakHeapObject weak_native_promise_;

  friend class MozjsUserObjectHolder<NativePromise>;
};

typedef MozjsUserObjectHolder<NativePromise> MozjsPromiseHolder;

template <>
struct TypeTraits<OpaqueHandle> {
  typedef MozjsPromiseHolder ConversionType;
  typedef const ScriptValue<Promise>* ReturnType;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_MOZJS_NATIVE_PROMISE_H_
