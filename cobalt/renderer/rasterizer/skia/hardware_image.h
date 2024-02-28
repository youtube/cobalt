// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"  // included for GrMipMapped
                                                   // alias

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

using GrContext = GrDirectContext;

// We use GL RGBA formats to indicate that a texture has 4 channels, but those
// 4 channels may not always strictly mean red, green, blue and alpha.  This
// enum is used to specify what format they are so that potentially different
// shaders can be selected.
enum AlternateRgbaFormat {
  AlternateRgbaFormat_UYVY,
};

typedef base::Callback<void(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target)>
    SubmitOffscreenCallback;

// Forwards ImageData methods on to TextureData methods.
class HardwareImageData : public render_tree::ImageData {
 public:
  HardwareImageData(std::unique_ptr<backend::TextureDataEGL> texture_data,
                    render_tree::PixelFormat pixel_format,
                    render_tree::AlphaFormat alpha_format);

  const render_tree::ImageDataDescriptor& GetDescriptor() const override;
  uint8_t* GetMemory() override;

  std::unique_ptr<backend::TextureDataEGL> PassTextureData();

 private:
  std::unique_ptr<backend::TextureDataEGL> texture_data_;
  render_tree::ImageDataDescriptor descriptor_;
};

class HardwareRawImageMemory : public render_tree::RawImageMemory {
 public:
  HardwareRawImageMemory(
      std::unique_ptr<backend::RawTextureMemoryEGL> raw_texture_memory);

  size_t GetSizeInBytes() const override;
  uint8_t* GetMemory() override;

  std::unique_ptr<backend::RawTextureMemoryEGL> PassRawTextureMemory();

 private:
  std::unique_ptr<backend::RawTextureMemoryEGL> raw_texture_memory_;
};

// A proxy object that can be used for inclusion in render trees.  When
// constructed, it also sends a message to the rasterizer's thread to have
// a corresponding backend image object constructed with the actual image data.
// The frontend image is what is actually returned from a call to
// HardwareResourceProvider::CreateImage(), but the backend object is what
// actually contains the texture data.
class HardwareFrontendImage : public SinglePlaneImage {
 public:
  HardwareFrontendImage(
      std::unique_ptr<HardwareImageData> image_data,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner);
  HardwareFrontendImage(
      const scoped_refptr<backend::ConstRawTextureMemoryEGL>&
          raw_texture_memory,
      intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner);
  HardwareFrontendImage(
      std::unique_ptr<backend::TextureEGL> texture,
      render_tree::AlphaFormat alpha_format,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      std::unique_ptr<math::RectF> content_region,
      scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner,
      base::Optional<AlternateRgbaFormat> alternate_rgba_format);
  HardwareFrontendImage(
      const scoped_refptr<render_tree::Node>& root,
      const SubmitOffscreenCallback& submit_offscreen_callback,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner);

  const math::Size& GetSize() const override { return size_; }

  // This method must only be called on the rasterizer thread.  This should not
  // be tricky to enforce since it is declared in skia::Image, and not Image.
  // The outside world deals only with Image objects and typically it is only
  // the skia render tree visitor that is aware of skia::Images.  Since the
  // skia render tree should only be visited on the rasterizer thread, this
  // restraint should always be satisfied naturally.
  const sk_sp<SkImage>& GetImage() const override;

  const backend::TextureEGL* GetTextureEGL() const override;

  const math::RectF* GetContentRegion() const override {
    return content_region_.get();
  }

  bool CanRenderInSkia() const override;

  bool EnsureInitialized() override;

  bool IsOpaque() const override { return is_opaque_; }

  base::Optional<AlternateRgbaFormat> alternate_rgba_format() {
    return alternate_rgba_format_;
  }

 private:
  ~HardwareFrontendImage() override;

  // Helper function to be called from the constructor.
  void InitializeBackend();

  // Track if we have any alpha or not, which can enable optimizations in the
  // case that alpha is not present.
  bool is_opaque_;

  // An optional rectangle, in pixel coordinates (with the top-left as the
  // origin) that indicates where in this image the valid content is contained.
  // Usually this is only set from platform-specific SbDecodeTargets.
  std::unique_ptr<math::RectF> content_region_;

  // In some cases where HardwareFrontendImage wraps a RGBA texture, the texture
  // actually contains pixel data in a non-RGBA format, like UYVY for example.
  // In this case, we track that in this member.  If this value is null, then
  // we are dealing with a normal RGBA texture.
  const base::Optional<AlternateRgbaFormat> alternate_rgba_format_;

  // We shadow the image dimensions so they can be quickly looked up from just
  // the frontend image object.
  const math::Size size_;

  // We keep track of a message loop which indicates the loop upon which we
  // can issue graphics commands.  Specifically, this is the message loop
  // where all HardwareBackendImage (described below) logic is executed
  // on.
  scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner_;

  // The HardwareBackendImage object is where all our rasterizer thread
  // specific objects live, such as the backend Skia graphics reference to
  // the texture object.  These items typically must be created, accessed and
  // destroyed all on the same thread, and so this object's methods should
  // always be executed on the rasterizer thread.  It is constructed when
  // the HardwareFrontendImage is constructed, but destroyed when
  // a message sent by HardwareFrontendImage's destructor is received by
  // the rasterizer thread.
  class HardwareBackendImage;
  std::unique_ptr<HardwareBackendImage> backend_image_;

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
      std::unique_ptr<HardwareRawImageMemory> raw_image_memory,
      const render_tree::MultiPlaneImageDataDescriptor& descriptor,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner);

  HardwareMultiPlaneImage(
      render_tree::MultiPlaneImageFormat format,
      const std::vector<scoped_refptr<HardwareFrontendImage> >& planes);

  const math::Size& GetSize() const override { return size_; }

  uint32 GetEstimatedSizeInBytes() const override {
    return estimated_size_in_bytes_;
  }

  render_tree::MultiPlaneImageFormat GetFormat() const override {
    return format_;
  }

  const backend::TextureEGL* GetTextureEGL(int plane_index) const override {
    return planes_[plane_index]->GetTextureEGL();
  }

  scoped_refptr<HardwareFrontendImage> GetHardwareFrontendImage(
      int plane_index) const {
    return planes_[plane_index];
  }

  // Always fallback to custom non-skia code for rendering multi-plane images.
  // The main reason to unconditionally fallback here is because Skia does a
  // check internally to see if GL_RED is supported, and if so it will use
  // GL_RED for 1-channel textures, and if not it will use GL_ALPHA for
  // 1-channel textures.  If we want to create textures like this manually (and
  // later wrap it into a Skia texture), we must know in advance which GL format
  // to set it up as, but Skia's GL_RED support decision is private information
  // that we can't access.  So, we choose instead to just not rely on Skia
  // for this so that we don't have to worry about format mismatches.
  bool CanRenderInSkia() const override { return false; }

  bool EnsureInitialized() override;

 private:
  ~HardwareMultiPlaneImage() override;

  const math::Size size_;
  uint32 estimated_size_in_bytes_;

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
