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
#ifndef COBALT_SCRIPT_MOZJS_WEAK_HEAP_OBJECT_H_
#define COBALT_SCRIPT_MOZJS_WEAK_HEAP_OBJECT_H_

#include "cobalt/script/mozjs/weak_heap_object_manager.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// This class implements a weak reference to a JSObject. The JSObject that an
// instance of this class is created with may get garbage collected. In that
// case, subsequent calls to WeakHeapObject::Get() will return a NULL pointer.
class WeakHeapObject {
 public:
  WeakHeapObject(JSContext* context, JS::HandleObject handle);
  WeakHeapObject(const WeakHeapObject& other);

  WeakHeapObject& operator=(const WeakHeapObject& rhs);
  JSObject* Get() const { return heap_; }
  void Trace(JSTracer* trace);

  ~WeakHeapObject();

 private:
  void Initialize(WeakHeapObjectManager* weak_heap_object_manager,
                  JSObject* object);

  WeakHeapObjectManager* weak_object_manager_;
  JS::Heap<JSObject*> heap_;

  friend class WeakHeapObjectManager;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_WEAK_HEAP_OBJECT_H_
