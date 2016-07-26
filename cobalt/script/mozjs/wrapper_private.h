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
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/mozjs/wrapper_factory.h"
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
  template <typename T>
  scoped_refptr<T> wrappable() const {
    return base::polymorphic_downcast<T*>(wrappable_.get());
  }

  JSObject* js_object_proxy() const { return wrapper_proxy_; }

  // Add/Remove a reference to the object. The object will be visited during
  // garbage collection.
  void AddReferencedObject(JS::HandleObject referee);
  void RemoveReferencedObject(JS::HandleObject referee);

  // Create a new WrapperPrivate instance and associate it with the wrapper.
  static void AddPrivateData(JS::HandleObject wrapper_proxy,
                             const scoped_refptr<Wrappable>& wrappable);

  // Get the WrapperPrivate associated with the given Wrappable. A new JSObject
  // and WrapperPrivate object may be created.
  static WrapperPrivate* GetFromWrappable(
      const scoped_refptr<Wrappable>& wrappable, JSContext* context,
      WrapperFactory* wrapper_factory);

  // Get the WrapperPrivate instance associated with this Wrapper object.
  static WrapperPrivate* GetFromWrapperObject(JS::HandleObject object);

  // Get the WrapperPrivate instance associated with the target of this proxy.
  static WrapperPrivate* GetFromProxyObject(JSContext* context,
                                            JS::HandleObject proxy_object);

  // Get the WrapperPrivate instance associated with the object, which may
  // be a proxy or a proxy target.
  static WrapperPrivate* GetFromObject(JSContext* context,
                                       JS::HandleObject object);

  // Called when the wrapper object is about to be deleted by the GC.
  static void Finalizer(JSFreeOp* /* free_op */, JSObject* object);

  // Trace callback called during garbage collection.
  static void Trace(JSTracer* trace, JSObject* object);

 private:
  typedef ScopedVector<JS::Heap<JSObject*> > ReferencedObjectVector;
  WrapperPrivate(const scoped_refptr<Wrappable>& wrappable,
                 JS::HandleObject wrapper_proxy);

  scoped_refptr<Wrappable> wrappable_;
  JS::Heap<JSObject*> wrapper_proxy_;
  ReferencedObjectVector referenced_objects_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_WRAPPER_PRIVATE_H_
