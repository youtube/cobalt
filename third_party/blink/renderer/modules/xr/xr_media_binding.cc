// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_media_binding.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_cylinder_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_equirect_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_media_cylinder_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_media_equirect_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_media_quad_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_quad_layer_init.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/xr/xr_cylinder_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_equirect_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_media_drawing_context.h"
#include "third_party/blink/renderer/modules/xr/xr_quad_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

XRMediaBinding* XRMediaBinding::Create(ScriptState* script_state,
                                       XRSession* session,
                                       ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRMediaBinding for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }
  return MakeGarbageCollected<XRMediaBinding>(session);
}

XRMediaBinding::XRMediaBinding(XRSession* session) : session_(session) {}

XRQuadLayer* XRMediaBinding::createQuadLayer(HTMLVideoElement* video,
                                             const XRMediaQuadLayerInit* init,
                                             ExceptionState& exception_state) {
  if (session_->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Session has ended.");
    return nullptr;
  }

  if (video->videoWidth() == 0 || video->videoHeight() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Video width or height is 0.");
    return nullptr;
  }

  if (!ValidateQuadLayerInit(init, exception_state)) {
    return nullptr;
  }

  auto* drawing_context =
      MakeGarbageCollected<XRMediaDrawingContext>(session_, video);

  auto* quad_init = XRQuadLayerInit::Create();
  quad_init->setSpace(init->space());
  quad_init->setViewPixelWidth(drawing_context->TextureWidth());
  quad_init->setViewPixelHeight(drawing_context->TextureHeight());
  quad_init->setLayout(init->layout());
  if (init->hasTransform()) {
    quad_init->setTransform(init->transform());
  }
  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(init->layout(), V8XRTextureType::Enum::kTexture,
                      session_->StereoscopicViews());
  double aspect_ratio =
      static_cast<double>(video->videoWidth()) / video->videoHeight();
  if (final_layout == V8XRLayerLayout::Enum::kStereoLeftRight) {
    aspect_ratio /= 2.0;
  } else if (final_layout == V8XRLayerLayout::Enum::kStereoTopBottom) {
    aspect_ratio *= 2.0;
  }

  float physical_width = 1.0f;
  float physical_height = 1.0f;

  if (init->hasWidth() && init->hasHeight()) {
    physical_width = init->width();
    physical_height = init->height();
  } else if (init->hasWidth()) {
    physical_width = init->width();
    physical_height = physical_width / aspect_ratio;
  } else if (init->hasHeight()) {
    physical_height = init->height();
    physical_width = physical_height * aspect_ratio;
  } else {
    physical_width = 1.0f;
    physical_height = physical_width / aspect_ratio;
  }

  quad_init->setWidth(physical_width);
  quad_init->setHeight(physical_height);

  return MakeGarbageCollected<XRQuadLayer>(session_, quad_init, final_layout,
                                           nullptr, drawing_context);
}

XRCylinderLayer* XRMediaBinding::createCylinderLayer(
    HTMLVideoElement* video,
    const XRMediaCylinderLayerInit* init,
    ExceptionState& exception_state) {
  if (session_->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Session has ended.");
    return nullptr;
  }

  if (video->videoWidth() == 0 || video->videoHeight() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Video width or height is 0.");
    return nullptr;
  }

  if (!ValidateCylinderLayerInit(init, exception_state)) {
    return nullptr;
  }

  auto* drawing_context =
      MakeGarbageCollected<XRMediaDrawingContext>(session_, video);

  auto* cyl_init = XRCylinderLayerInit::Create();
  cyl_init->setSpace(init->space());
  cyl_init->setViewPixelWidth(drawing_context->TextureWidth());
  cyl_init->setViewPixelHeight(drawing_context->TextureHeight());
  cyl_init->setLayout(init->layout());
  if (init->hasTransform()) {
    cyl_init->setTransform(init->transform());
  }
  if (init->hasRadius()) {
    cyl_init->setRadius(init->radius());
  }
  if (init->hasCentralAngle()) {
    cyl_init->setCentralAngle(init->centralAngle());
  }

  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(init->layout(), V8XRTextureType::Enum::kTexture,
                      session_->StereoscopicViews());
  double aspect_ratio =
      static_cast<double>(video->videoWidth()) / video->videoHeight();
  if (final_layout == V8XRLayerLayout::Enum::kStereoLeftRight) {
    aspect_ratio /= 2.0;
  } else if (final_layout == V8XRLayerLayout::Enum::kStereoTopBottom) {
    aspect_ratio *= 2.0;
  }

  float physical_aspect_ratio = aspect_ratio;
  if (init->hasAspectRatio()) {
    physical_aspect_ratio = init->aspectRatio();
  }
  cyl_init->setAspectRatio(physical_aspect_ratio);

  return MakeGarbageCollected<XRCylinderLayer>(session_, cyl_init, final_layout,
                                               nullptr, drawing_context);
}

XREquirectLayer* XRMediaBinding::createEquirectLayer(
    HTMLVideoElement* video,
    const XRMediaEquirectLayerInit* init,
    ExceptionState& exception_state) {
  if (session_->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Session has ended.");
    return nullptr;
  }

  if (video->videoWidth() == 0 || video->videoHeight() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Video width or height is 0.");
    return nullptr;
  }

  if (!ValidateEquirectLayerInit(init, exception_state)) {
    return nullptr;
  }

  auto* drawing_context =
      MakeGarbageCollected<XRMediaDrawingContext>(session_, video);

  auto* eq_init = XREquirectLayerInit::Create();
  eq_init->setSpace(init->space());
  eq_init->setViewPixelWidth(drawing_context->TextureWidth());
  eq_init->setViewPixelHeight(drawing_context->TextureHeight());
  eq_init->setLayout(init->layout());
  if (init->hasTransform()) {
    eq_init->setTransform(init->transform());
  }
  if (init->hasRadius()) {
    eq_init->setRadius(init->radius());
  }
  if (init->hasCentralHorizontalAngle()) {
    eq_init->setCentralHorizontalAngle(init->centralHorizontalAngle());
  }
  if (init->hasUpperVerticalAngle()) {
    eq_init->setUpperVerticalAngle(init->upperVerticalAngle());
  }
  if (init->hasLowerVerticalAngle()) {
    eq_init->setLowerVerticalAngle(init->lowerVerticalAngle());
  }

  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(init->layout(), V8XRTextureType::Enum::kTexture,
                      session_->StereoscopicViews());

  return MakeGarbageCollected<XREquirectLayer>(session_, eq_init, final_layout,
                                               nullptr, drawing_context);
}

void XRMediaBinding::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
