// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_WRAPPER_HANDLE_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_WRAPPER_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/mozjs-45/wrapper_private.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Implements Wrappable::WeakWrapperHandler and thus represents the cached
// JS wrapper for a given Wrappable. The Wrapper could be garbage collected
// at any time.
class MozjsWrapperHandle : public Wrappable::WeakWrapperHandle {
 public:
  explicit MozjsWrapperHandle(WrapperPrivate* wrapper_private) {
    DCHECK(wrapper_private);
    weak_wrapper_private_ = wrapper_private->AsWeakPtr();
  }

  static JSObject* GetObjectProxy(const Wrappable::WeakWrapperHandle* handle) {
    if (handle) {
      const MozjsWrapperHandle* mozjs_handle =
          base::polymorphic_downcast<const MozjsWrapperHandle*>(handle);
      if (mozjs_handle->weak_wrapper_private_) {
        return mozjs_handle->weak_wrapper_private_->js_object_proxy();
      }
    }
    return NULL;
  }

 private:
  base::WeakPtr<WrapperPrivate> weak_wrapper_private_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_WRAPPER_HANDLE_H_
