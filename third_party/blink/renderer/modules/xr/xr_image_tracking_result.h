// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_IMAGE_TRACKING_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_IMAGE_TRACKING_RESULT_H_

#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace gfx {
class Transform;
}

namespace blink {

class XRSession;
class XRSpace;

class XRImageTrackingResult : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRImageTrackingResult(
      XRSession* session,
      const device::mojom::blink::XRTrackedImageData& image_tracking_result);

  XRSpace* imageSpace() const;
  absl::optional<gfx::Transform> MojoFromObject() const;

  device::mojom::blink::XRNativeOriginInformationPtr NativeOrigin() const;

  uint32_t index() { return index_; }

  float measuredWidthInMeters() { return width_in_meters_; }

  const String& trackingState() { return tracking_state_string_; }

  bool IsStationary() const { return false; }

  void Trace(Visitor* visitor) const override;

 private:
  Member<XRSession> session_;
  uint32_t index_;
  String tracking_state_string_;
  absl::optional<device::Pose> mojo_from_this_;
  float width_in_meters_;

  // Cached image space - it will be created by `imageSpace()` if it's not set.
  mutable Member<XRSpace> image_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_IMAGE_TRACKING_RESULT_H_
