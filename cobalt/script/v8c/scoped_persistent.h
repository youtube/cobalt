// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
      : traced_global_(isolate, handle) {}

  ScopedPersistent(v8::Isolate* isolate, v8::MaybeLocal<T> maybe) {
    v8::Local<T> local;
    if (maybe.ToLocal(&local)) {
      traced_global_.Reset(isolate, local);
    }
  }

  ~ScopedPersistent() { Clear(); }

  v8::Local<T> NewLocal(v8::Isolate* isolate) const {
    return v8::Local<T>::New(isolate, traced_global_);
  }

  bool IsEmpty() const { return traced_global_.IsEmpty(); }

  void Set(v8::Isolate* isolate, v8::Local<T> handle) {
    traced_global_.Reset(isolate, handle);
  }

  void Clear() { traced_global_.Reset(); }

  bool operator==(const ScopedPersistent<T>& other) {
    return traced_global_ == other.handle_;
  }

  template <class S>
  bool operator==(const v8::Local<S> other) const {
    return traced_global_ == other;
  }

  v8::TracedGlobal<T>& Get() { return traced_global_; }
  const v8::TracedGlobal<T>& Get() const { return traced_global_; }
  const v8::TracedGlobal<T>& traced_global() const {
    DCHECK(!traced_global_.IsEmpty());
    return traced_global_;
  }

  v8::TracedGlobal<T>* traced_global_ptr() { return &traced_global_; }

 private:
  v8::TracedGlobal<T> traced_global_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_SCOPED_PERSISTENT_H_
