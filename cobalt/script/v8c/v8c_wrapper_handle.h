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

#ifndef COBALT_SCRIPT_V8C_V8C_WRAPPER_HANDLE_H_
#define COBALT_SCRIPT_V8C_V8C_WRAPPER_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/wrapper_private.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace script {
namespace v8c {

// Implements Wrappable::WeakWrapperHandler and thus represents the cached
// JS wrapper for a given Wrappable. The Wrapper could be garbage collected
// at any time.
struct V8cWrapperHandle : public Wrappable::WeakWrapperHandle {
 public:
  static v8::MaybeLocal<v8::Object> MaybeGetObject(
      v8::Isolate* isolate, const Wrappable::WeakWrapperHandle* handle) {
    WrapperPrivate* weak_wrapper_private =
        MaybeGetWrapperPrivate(isolate, handle);
    if (!weak_wrapper_private) {
      return {};
    }
    return weak_wrapper_private->wrapper();
  }

  static WrapperPrivate* MaybeGetWrapperPrivate(
      v8::Isolate* isolate, const Wrappable::WeakWrapperHandle* handle) {
    if (!handle) {
      return nullptr;
    }
    const V8cWrapperHandle* v8c_handle =
        base::polymorphic_downcast<const V8cWrapperHandle*>(handle);
    return v8c_handle->weak_wrapper_private_;
  }

  explicit V8cWrapperHandle(WrapperPrivate* wrapper_private) {
    DCHECK(wrapper_private);
    weak_wrapper_private_ = wrapper_private->AsWeakPtr();
  }

  base::WeakPtr<WrapperPrivate> weak_wrapper_private_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_WRAPPER_HANDLE_H_
