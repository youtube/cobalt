// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MEDIA_DRAWING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MEDIA_DRAWING_CONTEXT_H_

#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_drawing_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class HTMLVideoElement;
class XRCompositionLayer;
class XRSession;

class XRMediaDrawingContext : public XRLayerDrawingContext {
 public:
  XRMediaDrawingContext(XRSession* session, HTMLVideoElement* video);
  ~XRMediaDrawingContext() override;

  void OnFrameStart() override;
  void OnFrameEnd() override;
  void SetCompositionLayer(XRCompositionLayer* layer) override;
  uint16_t TextureWidth() const override;
  uint16_t TextureHeight() const override;
  uint16_t TextureArrayLength() const override { return 1; }
  bool TextureWasQueried() const override { return content_changed_; }
  bool ShouldFlipY() const override { return true; }
  bool NeedsRasterAccess() const override { return true; }
  bool IsMediaLayer() const override { return true; }

  XRSession* session() const override { return session_.Get(); }
  std::unique_ptr<SharedImageHolder> TransferToSharedImageHolder() override;
  std::unique_ptr<SharedImageHolder> DoneWithSharedBuffer() override;
  XRFrameTransportDelegate* GetTransportDelegate() override {
    return frame_transport_delegate_.Get();
  }
  void Trace(Visitor* visitor) const override;

 private:
  Member<XRSession> session_;
  Member<HTMLVideoElement> video_;
  Member<XRCompositionLayer> layer_;
  gpu::SyncToken sync_token_;
  Member<XRFrameTransportDelegate> frame_transport_delegate_;
  uint16_t width_ = 0;
  uint16_t height_ = 0;
  uint16_t max_texture_size_ = 2048;
  bool content_changed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MEDIA_DRAWING_CONTEXT_H_
