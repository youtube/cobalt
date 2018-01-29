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

#ifndef COBALT_SCRIPT_MOZJS_45_WEAK_HEAP_OBJECT_H_
#define COBALT_SCRIPT_MOZJS_45_WEAK_HEAP_OBJECT_H_

#include "cobalt/script/mozjs-45/weak_heap_object_manager.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// This class implements a weak reference to a JS::Value. The JS::Value that an
// instance of this class is created with may get garbage collected. In that
// case, subsequent calls to WeakHeapObject::Get() will return a NULL pointer.
class WeakHeapObject {
 public:
  WeakHeapObject(JSContext* context, JS::HandleValue value);
  WeakHeapObject(const WeakHeapObject& other);
  WeakHeapObject& operator=(const WeakHeapObject& rhs);

  ~WeakHeapObject();

  const JS::Value& GetValue() const { return value_.get(); }
  JSObject* GetObject() const {
    return value_.toObjectOrNull();
  }

  void Trace(JSTracer* js_tracer);
  bool IsObject() const;
  bool IsGcThing() const;

  // Whether the value was a GC Thing and has been actually GC'd.
  bool WasCollected() const;

 private:
  void Initialize(WeakHeapObjectManager* weak_heap_object_manager,
                  const JS::Value& value);

  void UpdateWeakPointerAfterGc();

  WeakHeapObjectManager* weak_object_manager_;
  JS::Heap<JS::Value> value_;
  bool was_collected_;

  friend class WeakHeapObjectManager;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_WEAK_HEAP_OBJECT_H_
