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

#ifndef COBALT_SCRIPT_V8C_NATIVE_PROMISE_H_
#define COBALT_SCRIPT_V8C_NATIVE_PROMISE_H_

#include "cobalt/script/promise.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"

namespace cobalt {
namespace script {
namespace v8c {

// Shared functionality for NativePromise<T>. Does not implement the Resolve
// function, since that needs to be specialized for Promise<T>.
template <typename T>
class NativePromiseBase : public Promise<T> {
 public:
  // ScriptObject boilerplate.
  typedef Promise<T> BaseType;

  void Reject() const override { NOTIMPLEMENTED(); }
  void Reject(SimpleExceptionType exception) const override {
    NOTIMPLEMENTED();
  }
  void Reject(const scoped_refptr<ScriptException>& result) const override {
    NOTIMPLEMENTED();
  }
};

// Implements the Resolve() function for T != void.
template <typename T>
class NativePromise : public NativePromiseBase<T> {
 public:
  void Resolve(const T& value) const override { NOTIMPLEMENTED(); }
};

// Implements the Resolve() function for T == void.
template <>
class NativePromise<void> : public NativePromiseBase<void> {
 public:
  void Resolve() const override { NOTIMPLEMENTED(); }
};

template <typename T>
struct TypeTraits<NativePromise<T> > {
  typedef V8cUserObjectHolder<NativePromise<T> > ConversionType;
  typedef const ScriptValue<Promise<T> >* ReturnType;
};

// Promise<T> -> JSValue
// Note that JSValue -> Promise<T> is not yet supported.
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<Promise<T> >* promise_holder,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// Explicitly defer to the const version here so that a more generic non-const
// version of this function does not get called instead, in the case that
// |promise_holder| is not const.
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      ScriptValue<Promise<T> >* promise_holder,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// Destroys |promise_holder| as soon as the conversion is done.
// This is useful when a wrappable is not interested in retaining a reference
// to a promise, typically when a promise is resolved or rejected synchronously.
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      scoped_ptr<ScriptValue<Promise<T> > > promise_holder,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_NATIVE_PROMISE_H_
