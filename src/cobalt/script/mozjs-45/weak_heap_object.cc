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

#include "cobalt/script/mozjs-45/weak_heap_object.h"

#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "third_party/mozjs-45/js/src/gc/Marking.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

WeakHeapObject::WeakHeapObject(JSContext* context, JS::HandleValue value)
    : was_collected_(false) {
  MozjsGlobalEnvironment* global_environment =
      MozjsGlobalEnvironment::GetFromContext(context);
  Initialize(global_environment->weak_object_manager(), value);
}

WeakHeapObject::WeakHeapObject(JSContext* context, JS::HandleObject object)
    : was_collected_(false) {
  MozjsGlobalEnvironment* global_environment =
      MozjsGlobalEnvironment::GetFromContext(context);
  Initialize(global_environment->weak_object_manager(),
             JS::ObjectValue(*object));
}

WeakHeapObject::WeakHeapObject(const WeakHeapObject& other)
    : was_collected_(other.was_collected_) {
  Initialize(other.weak_object_manager_, other.value_);
}

WeakHeapObject& WeakHeapObject::operator=(const WeakHeapObject& rhs) {
  was_collected_ = rhs.was_collected_;
  Initialize(rhs.weak_object_manager_, rhs.value_);
  return *this;
}

void WeakHeapObject::Trace(JSTracer* trace) {
  if (!value_.isNullOrUndefined()) {
    JS_CallValueTracer(trace, &value_, "WeakHeapObject::Trace");
  }
}

bool WeakHeapObject::IsObject() const {
  return value_.isObject();
}

bool WeakHeapObject::IsGcThing() const {
  // We have to check IsNull(), because null is apparently a GC Thing.
  return (!IsNull() && value_.isGCThing());
}

bool WeakHeapObject::WasCollected() const {
  return (was_collected_ && IsNull());
}

WeakHeapObject::~WeakHeapObject() {
  // It's safe to call StopTracking even if StartTracking wasn't called. the
  // WeakObjectManager handles the case where it's not currently tracking the
  // WeakHeapObject.
  weak_object_manager_->StopTracking(this);
}

void WeakHeapObject::Initialize(WeakHeapObjectManager* weak_heap_object_manager,
                                const JS::Value& value) {
  weak_object_manager_ = weak_heap_object_manager;
  value_ = value;

  // Don't bother registering if not a GC thing.
  if (IsGcThing()) {
    weak_object_manager_->StartTracking(this);
  }
}

bool WeakHeapObject::IsNull() const {
  return value_.isNullOrUndefined();
}

void WeakHeapObject::UpdateWeakPointerAfterGc() {
  if (js::gc::IsAboutToBeFinalizedUnbarriered(value_.unsafeGet())) {
    DCHECK(IsGcThing());
    was_collected_ = true;
    value_.setNull();
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
