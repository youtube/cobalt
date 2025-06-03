// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRUSTED_OBJECT_INL_H_
#define V8_OBJECTS_TRUSTED_OBJECT_INL_H_

#include "src/objects/instance-type-inl.h"
#include "src/objects/trusted-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(TrustedObject)
OBJECT_CONSTRUCTORS_IMPL(TrustedObject, HeapObject)

CAST_ACCESSOR(ExposedTrustedObject)
OBJECT_CONSTRUCTORS_IMPL(ExposedTrustedObject, TrustedObject)

void ExposedTrustedObject::init_self_indirect_pointer(LocalIsolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  InitSelfIndirectPointerField(kSelfIndirectPointerOffset, isolate);
#endif
}

IndirectPointerHandle ExposedTrustedObject::self_indirect_pointer() const {
#ifdef V8_ENABLE_SANDBOX
  return ReadField<IndirectPointerHandle>(kSelfIndirectPointerOffset);
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TRUSTED_OBJECT_INL_H_
