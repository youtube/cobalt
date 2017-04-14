// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_IMAGE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_IMAGE_H_

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread_checker.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

typedef base::Callback<void(const scoped_refptr<render_tree::Node>& render_tree,
                            const scoped_refptr<backend::RenderTarget>&
                                render_target)> SubmitOffscreenCallback;

// Wraps a Cobalt backend::TextureEGL with a Skia GrTexture, and returns the
// Skia ref-counted GrTexture object (that takes ownership of the cobalt
// texture).
GrTexture* CobaltTextureToSkiaTexture(
    GrContext* gr_context, scoped_ptr<backend::TextureEGL> cobalt_texture);

// Forwards ImageData methods on to TextureData methods.
class HardwareImageData : public render_tree::ImageData {
 public:
  HardwareImageData(scoped_ptr<backend::TextureDataEGL> texture_data,
                    render_tree::PixelFormat pixel_format,
                    render_tree::AlphaFormat alpha_format);

  const render_tree::ImageDataDescriptor& GetDescriptor() const OVERRIDE;
  uint8_t* GetMemory() OVERRIDE;

  scoped_ptr<backend::TextureDataEGL> PassTextureData();

 private:
  scoped_ptr<backend::TextureDataEGL> texture_data_;
  render_tree::ImageDataDescriptor descriptor_;
};

class HardwareRawImageMemory : public render_tree::RawImageMemory {
 public:
  HardwareRawImageMemory(
      scoped_ptr<backend::RawTextureMemoryEGL> raw_texture_memory);

  size_t GetSizeInBytes() const OVERRIDE;
  uint8_t* GetMemory() OVERRIDE;

  scoped_ptr<backend::RawTextureMemoryEGL> PassRawTextureMemory();

 private:
  scoped_ptr<backend::RawTextureMemoryEGL> raw_texture_memory_;
};

// A proxy object that can be used for inclusion in render trees.  When
// constructed, it also sends a message to the rasterizer's thread to have
// a corresponding backend image object constructed with the actual image data.
// The frontend image is what is actually returned from a call to
// HardwareResourceProvider::CreateImage(), but the backend object is what
// actually contains the texture data.
class HardwareFrontendImage : public SinglePlaneImage {
 public:
  HardwareFrontendImage(scoped_ptr<HardwareImageData> image_data,
                        backend::GraphicsContextEGL* cobalt_context,
                        GrContext* gr_context,
                        MessageLoop* rasterizer_message_loop);
  HardwareFrontendImage(const scoped_refptr<backend::ConstRawTextureMemoryEGL>&
                            raw_texture_memory,
                        intptr_t offset,
                        const render_tree::ImageDataDescriptor& descriptor,
                        backend::GraphicsContextEGL* cobalt_context,
                        GrContext* gr_context,
                        MessageLoop* rasterizer_message_loop);
  HardwareFrontendImage(scoped_ptr<backend::TextureEGL> texture,
                        render_tree::AlphaFormat alpha_format,
                        backend::GraphicsContextEGL* cobalt_context,
                        GrContext* gr_context,
                        scoped_ptr<math::Rect> content_region,
                        MessageLoop* rasterizer_message_loop);
  HardwareFrontendImage(
      const scoped_refptr<render_tree::Node>& root,
      const SubmitOffscreenCallback& submit_offscreen_callback,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      MessageLoop* rasterizer_message_loop);

  const math::Size& GetSize() const OVERRIDE { return size_; }

  // This method must only be called on the rasterizer thread.  This should not
  // be tricky to enforce since it is declared in skia::Image, and not Image.
  // The outside world deals only with Image objects and typically it is only
  // the skia render tree visitor that is aware of skia::Images.  Since the
  // skia render tree should only be visited on the rasterizer thread, this
  // restraint should always be satisfied naturally.
  const SkBitmap* GetBitmap() const OVERRIDE;

  const backend::TextureEGL* GetTextureEGL() const OVERRIDE;

  const math::Rect* GetContentRegion() const OVERRIDE {
    return content_region_.get();
  }

  bool CanRenderInSkia() const OVERRIDE;

  bool EnsureInitialized() OVERRIDE;

  bool IsOpaque() const OVERRIDE { return is_opaque_; }

 private:
  ~HardwareFrontendImage() OVERRIDE;

  // The following Initialize functions will construct |backend_image_|, but
  // only if |backend_image_| is ever needed by the rasterizer thread.
  void InitializeBackendImageFromImageData(
      scoped_ptr<HardwareImageData> image_data,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context);
  void InitializeBackendImageFromRawImageData(
      const scoped_refptr<backend::ConstRawTextureMemoryEGL>&
          raw_texture_memory,
      intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context);
  void InitializeBackendImageFromTexture(GrContext* gr_context);
  void InitializeBackendImageFromRenderTree(
      const scoped_refptr<render_tree::Node>& root,
      const SubmitOffscreenCallback& submit_offscreen_callback,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context);

  // Track if we have any alpha or not, which can enable optimizations in the
  // case that alpha is not present.
  bool is_opaque_;

  // An optional rectangle, in pixel coordinates (with the top-left as the
  // origin) that indicates where in this image the valid content is contained.
  // Usually this is only set from platform-specific SbDecodeTargets.
  scoped_ptr<math::Rect> content_region_;

  // We shadow the image dimensions so they can be quickly looked up from just
  // the frontend image object.
  const math::Size size_;

  // We keep track of a message loop which indicates the loop upon which we
  // can issue graphics commands.  Specifically, this is the message loop
  // where all HardwareBackendImage (described below) logic is executed
  // on.
  MessageLoop* rasterizer_message_loop_;

  // The HardwareBackendImage object is where all our rasterizer thread
  // specific objects live, such as the backend Skia graphics reference to
  // the texture object.  These items typically must be created, accessed and
  // destroyed all on the same thread, and so this object's methods should
  // always be executed on the rasterizer thread.  It is constructed when
  // the HardwareFrontendImage is constructed, but destroyed when
  // a message sent by HardwareFrontendImage's destructor is received by
  // the rasterizer thread.
  class HardwareBackendImage;
  scoped_ptr<HardwareBackendImage> backend_image_;

  // This closure binds the backend image construction parameters so that we
  // can delay construction of it until it is accessed by the rasterizer thread.
  // If this closure is not null, then |backend_image_| should be null, and
  // running it will result in the creation of |backend_image_|.
  base::Closure initialize_backend_image_;
};

// Multi-plane images are implemented as collections of single plane images.
class HardwareMultiPlaneImage : public MultiPlaneImage {
 public:
  HardwareMultiPlaneImage(
      scoped_ptr<HardwareRawImageMemory> raw_image_memory,
      const render_tree::MultiPlaneImageDataDescriptor& descriptor,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      MessageLoop* rasterizer_message_loop);

  const math::Size& GetSize() const OVERRIDE { return size_; }
  render_tree::MultiPlaneImageFormat GetFormat() const OVERRIDE {
    return format_;
  }

  // Forward the request to get a bitmap to the internal single-plane image
  // specified by plane_index.
  const SkBitmap* GetBitmap(int plane_index) const OVERRIDE {
    return planes_[plane_index]->GetBitmap();
  }
  const backend::TextureEGL* GetTextureEGL(int plane_index) const OVERRIDE {
    return planes_[plane_index]->GetTextureEGL();
  }

  bool EnsureInitialized() OVERRIDE;

 private:
  ~HardwareMultiPlaneImage() OVERRIDE;

  const math::Size size_;

  render_tree::MultiPlaneImageFormat format_;

  // We maintain a single-plane image for each plane of this multi-plane image.
  scoped_refptr<HardwareFrontendImage>
      planes_[render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes];
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_IMAGE_H_
