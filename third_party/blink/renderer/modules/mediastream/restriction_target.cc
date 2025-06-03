// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"

#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/mediastream/media_devices.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromise RestrictionTarget::fromElement(ScriptState* script_state,
                                             Element* element,
                                             ExceptionState& exception_state) {
  return SubCaptureTarget::fromElement(
      script_state, element, exception_state,
      SubCaptureTarget::Type::kRestrictionTarget);
}

RestrictionTarget::RestrictionTarget(String id)
    : SubCaptureTarget(SubCaptureTarget::Type::kRestrictionTarget,
                       std::move(id)) {}

}  // namespace blink
