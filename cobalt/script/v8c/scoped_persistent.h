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

#ifndef COBALT_SCRIPT_V8C_SCOPED_PERSISTENT_H_
#define COBALT_SCRIPT_V8C_SCOPED_PERSISTENT_H_

#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

template <typename T>
class ScopedPersistent {
 public:
  ScopedPersistent() {}

  ScopedPersistent(v8::Isolate* isolate, v8::Local<T> handle)
      : handle_(isolate, handle) {}

  ScopedPersistent(v8::Isolate* isolate, v8::MaybeLocal<T> maybe) {
    v8::Local<T> local;
    if (maybe.ToLocal(&local)) {
      handle_.Reset(isolate, local);
    }
  }

  ~ScopedPersistent() { Clear(); }

  v8::Local<T> NewLocal(v8::Isolate* isolate) const {
    return v8::Local<T>::New(isolate, handle_);
  }

  template <typename P>
  void SetWeak(P* parameters, void (*callback)(const v8::WeakCallbackInfo<P>&),
               v8::WeakCallbackType type = v8::WeakCallbackType::kParameter) {
    handle_.SetWeak(parameters, callback, type);
  }

  void SetWeak() { handle_.SetWeak(); }

  void ClearWeak() { handle_.template ClearWeak<void>(); }

  bool IsEmpty() const { return handle_.IsEmpty(); }
  bool IsWeak() const { return handle_.IsWeak(); }

  void Set(v8::Isolate* isolate, v8::Local<T> handle) {
    handle_.Reset(isolate, handle);
  }

  void Clear() { handle_.Reset(); }

  bool operator==(const ScopedPersistent<T>& other) {
    return handle_ == other.handle_;
  }

  template <class S>
  bool operator==(const v8::Local<S> other) const {
    return handle_ == other;
  }

  v8::Persistent<T>& Get() { return handle_; }
  const v8::Persistent<T>& Get() const { return handle_; }

 private:
  v8::Persistent<T> handle_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_SCOPED_PERSISTENT_H_
