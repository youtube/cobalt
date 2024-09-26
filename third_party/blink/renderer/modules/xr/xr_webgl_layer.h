// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_webgl_layer_init.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
class HTMLCanvasElement;
class WebGLFramebuffer;
class WebGLRenderingContextBase;
class XRSession;
class XRViewport;

class XRWebGLLayer final : public XRLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRWebGLLayer(XRSession*,
               WebGLRenderingContextBase*,
               scoped_refptr<XRWebGLDrawingBuffer>,
               WebGLFramebuffer*,
               double framebuffer_scale,
               bool ignore_depth_values);
  ~XRWebGLLayer() override;

  static XRWebGLLayer* Create(XRSession*,
                              const V8XRWebGLRenderingContext*,
                              const XRWebGLLayerInit*,
                              ExceptionState&);

  WebGLRenderingContextBase* context() const { return webgl_context_; }

  WebGLFramebuffer* framebuffer() const { return framebuffer_; }
  uint32_t framebufferWidth() const;
  uint32_t framebufferHeight() const;

  bool antialias() const;
  bool ignoreDepthValues() const { return ignore_depth_values_; }

  XRViewport* getViewport(XRView*);

  static double getNativeFramebufferScaleFactor(XRSession* session);

  XRViewport* GetViewportForEye(device::mojom::blink::XREye);

  void UpdateViewports();

  HTMLCanvasElement* output_canvas() const;

  // Returns WebGLTexture (actually a WebGLUnownedTexture instance)
  // corresponding to the camera image.
  // The texture is owned by the XRWebGLLayer and will be freed in OnFrameEnd().
  // When the texture is deleted by the layer, the returned object will have its
  // texture name set to 0 to avoid using stale texture names in case the user
  // code still holds references to this object.
  // The consumers should not attempt to delete the texture themselves.
  WebGLTexture* GetCameraTexture();

  void OnFrameStart(
      const absl::optional<gpu::MailboxHolder>& buffer_mailbox_holder,
      const absl::optional<gpu::MailboxHolder>& camera_image_mailbox_holder);
  void OnFrameEnd();
  void OnResize();

  // Called from XRSession::OnFrame handler. Params are background texture
  // mailbox holder and its size respectively.
  void HandleBackgroundImage(const gpu::MailboxHolder&, const gfx::Size&) {}

  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage();

  void Trace(Visitor*) const override;

 private:
  uint32_t GetBufferTextureId(
      const absl::optional<gpu::MailboxHolder>& buffer_mailbox_holder);

  void BindCameraBufferTexture(
      const absl::optional<gpu::MailboxHolder>& buffer_mailbox_holder);

  Member<XRViewport> left_viewport_;
  Member<XRViewport> right_viewport_;

  Member<WebGLRenderingContextBase> webgl_context_;
  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer_;
  Member<WebGLFramebuffer> framebuffer_;

  double framebuffer_scale_ = 1.0;
  bool viewports_dirty_ = true;
  bool is_direct_draw_frame = false;
  bool ignore_depth_values_ = false;

  uint32_t clean_frame_count = 0;

  uint32_t camera_image_texture_id_;
  // WebGL texture that points to the |camera_image_texture_|. Must be notified
  // via a call to |WebGLUnownedTexture::OnGLDeleteTextures()| when
  // |camera_image_texture_id_| is deleted.
  Member<WebGLUnownedTexture> camera_image_texture_;

  absl::optional<gpu::MailboxHolder> camera_image_mailbox_holder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_
