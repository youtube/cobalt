// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_

#include <memory>
#include <string>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class XRRigidTransform;

class XRReferenceSpace : public XRSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static device::mojom::blink::XRReferenceSpaceType StringToReferenceSpaceType(
      const String& reference_space_type);

  XRReferenceSpace(XRSession* session,
                   device::mojom::blink::XRReferenceSpaceType type);
  XRReferenceSpace(XRSession* session,
                   XRRigidTransform* origin_offset,
                   device::mojom::blink::XRReferenceSpaceType type);
  ~XRReferenceSpace() override;

  absl::optional<gfx::Transform> NativeFromViewer(
      const absl::optional<gfx::Transform>& mojo_from_viewer) const override;

  absl::optional<gfx::Transform> MojoFromNative() const override;

  bool IsStationary() const override;

  gfx::Transform NativeFromOffsetMatrix() const override;
  gfx::Transform OffsetFromNativeMatrix() const override;

  // We override getPose to ensure that the viewer pose in viewer space returns
  // the identity pose instead of the result of multiplying inverse matrices.
  XRPose* getPose(const XRSpace* other_space) const override;

  device::mojom::blink::XRReferenceSpaceType GetType() const;

  XRReferenceSpace* getOffsetReferenceSpace(XRRigidTransform* transform) const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(reset, kReset)

  device::mojom::blink::XRNativeOriginInformationPtr NativeOrigin() const final;

  std::string ToString() const override;

  void Trace(Visitor*) const override;

  virtual void OnReset();

 private:
  virtual XRReferenceSpace* cloneWithOriginOffset(
      XRRigidTransform* origin_offset) const;

  // Updates the mojo_from_floor_ transform to match the one present in the
  // latest display parameters of a session.
  void SetMojoFromFloor() const;

  mutable uint32_t stage_parameters_id_ = 0;

  // Floor from mojo (aka local-floor_from_mojo) transform.
  mutable std::unique_ptr<gfx::Transform> mojo_from_floor_;
  Member<XRRigidTransform> origin_offset_;
  device::mojom::blink::XRReferenceSpaceType type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_
