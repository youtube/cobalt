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
#ifndef COBALT_SCRIPT_MOZJS_WRAPPER_PRIVATE_H_
#define COBALT_SCRIPT_MOZJS_WRAPPER_PRIVATE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Contains private data associated with a JSObject representing a JS wrapper
// for a Cobalt platform object. There should be a one-to-one mapping of such
// JSObjects and WrapperPrivate instances, and the corresponding WrapperPrivate
// must be destroyed when its JSObject is garbage collected.
class WrapperPrivate : public base::SupportsWeakPtr<WrapperPrivate> {
 public:
  const scoped_refptr<Wrappable>& wrappable() const { return wrappable_; }
  JSObject* js_object() const { return wrapper_; }

  static void AddPrivateData(JS::HandleObject wrapper,
                             const scoped_refptr<Wrappable>& wrappable) {
    WrapperPrivate* private_data = new WrapperPrivate(wrappable, wrapper);
    JS_SetPrivate(wrapper, private_data);
    DCHECK_EQ(JS_GetPrivate(wrapper), private_data);
  }

  static void Finalizer(JSFreeOp* /* free_op */, JSObject* object) {
    WrapperPrivate* wrapper_private =
        reinterpret_cast<WrapperPrivate*>(JS_GetPrivate(object));
    DCHECK(wrapper_private);
    delete wrapper_private;
  }

 private:
  WrapperPrivate(const scoped_refptr<Wrappable>& wrappable,
                 JS::HandleObject wrapper)
      : wrappable_(wrappable), wrapper_(wrapper) {}

  scoped_refptr<Wrappable> wrappable_;
  JSObject* wrapper_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_WRAPPER_PRIVATE_H_
