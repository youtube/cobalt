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

#include "cobalt/script/mozjs/weak_heap_object.h"

#include "cobalt/script/mozjs/mozjs_global_object_proxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

WeakHeapObject::WeakHeapObject(JSContext* context, JS::HandleObject handle) {
  MozjsGlobalObjectProxy* global_environment =
      MozjsGlobalObjectProxy::GetFromContext(context);
  Initialize(global_environment->weak_object_manager(), handle);
}

WeakHeapObject::WeakHeapObject(const WeakHeapObject& other) {
  Initialize(other.weak_object_manager_, other.heap_);
}

WeakHeapObject& WeakHeapObject::operator=(const WeakHeapObject& rhs) {
  Initialize(rhs.weak_object_manager_, rhs.heap_);
  return *this;
}

void WeakHeapObject::Trace(JSTracer* trace) {
  if (heap_) {
    JS_CallHeapObjectTracer(trace, &heap_, "WeakHeapObject::Trace");
  }
}

WeakHeapObject::~WeakHeapObject() {
  // It's safe to call StopTracking even if StartTracking wasn't called. the
  // WeakObjectManager handles the case where it's not currently tracking the
  // WeakHeapObject.
  weak_object_manager_->StopTracking(this);
}

void WeakHeapObject::Initialize(WeakHeapObjectManager* weak_heap_object_manager,
                                JSObject* object) {
  weak_object_manager_ = weak_heap_object_manager;
  heap_ = object;
  // Don't bother registering if the pointer is already NULL.
  if (heap_) {
    weak_object_manager_->StartTracking(this);
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
