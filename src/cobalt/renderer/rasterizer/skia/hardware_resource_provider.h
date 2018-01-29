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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RESOURCE_PROVIDER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RESOURCE_PROVIDER_H_

#include <string>
#include <vector>

#include "cobalt/math/size.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/rasterizer/skia/hardware_image.h"
#include "cobalt/renderer/rasterizer/skia/text_shaper.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/ports/SkFontMgr.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// This class must be thread-safe and capable of creating resources that
// are to be consumed by this skia hardware rasterizer.  It must be constructed
// on the thread that will be visiting submitted render trees.
class HardwareResourceProvider : public render_tree::ResourceProvider {
 public:
  HardwareResourceProvider(backend::GraphicsContextEGL* cobalt_context,
                           GrContext* gr_context,
                           SubmitOffscreenCallback submit_offscreen_callback,
                           bool purge_skia_font_caches_on_destruction);
  ~HardwareResourceProvider() override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<HardwareResourceProvider>();
  }

  void Finish() override;

  bool PixelFormatSupported(render_tree::PixelFormat pixel_format) override;
  bool AlphaFormatSupported(render_tree::AlphaFormat alpha_format) override;

  scoped_ptr<render_tree::ImageData> AllocateImageData(
      const math::Size& size, render_tree::PixelFormat pixel_format,
      render_tree::AlphaFormat alpha_format) override;

  scoped_refptr<render_tree::Image> CreateImage(
      scoped_ptr<render_tree::ImageData> pixel_data) override;

#if SB_HAS(GRAPHICS)

  scoped_refptr<render_tree::Image> CreateImageFromSbDecodeTarget(
      SbDecodeTarget decode_target) override;

  // Return the associated SbDecodeTargetProvider with the ResourceProvider,
  // if it exists.  Returns NULL if SbDecodeTarget is not supported.
  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
    return &decode_target_graphics_context_provider_;
  }

  // Whether SbDecodeTargetIsSupported or not.
  bool SupportsSbDecodeTarget() override { return true; }

#endif  // SB_HAS(GRAPHICS)

  scoped_ptr<render_tree::RawImageMemory> AllocateRawImageMemory(
      size_t size_in_bytes, size_t alignment) override;

  scoped_refptr<render_tree::Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
      const render_tree::MultiPlaneImageDataDescriptor& descriptor) override;

  bool HasLocalFontFamily(const char* font_family_name) const override;

  scoped_refptr<render_tree::Typeface> GetLocalTypeface(
      const char* font_family_name, render_tree::FontStyle font_style) override;

  scoped_refptr<render_tree::Typeface> GetLocalTypefaceByFaceNameIfAvailable(
      const char* font_face_name) override;

  scoped_refptr<render_tree::Typeface> GetCharacterFallbackTypeface(
      int32 character, render_tree::FontStyle font_style,
      const std::string& language) override;

  // This resource provider uses ots (OpenTypeSanitizer) to sanitize the raw
  // typeface data and skia to generate the typeface. It supports TrueType,
  // OpenType, and WOFF data formats.
  scoped_refptr<render_tree::Typeface> CreateTypefaceFromRawData(
      scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data,
      std::string* error_string) override;

  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      render_tree::FontProvider* font_provider) override;

  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const std::string& utf8_string,
      const scoped_refptr<render_tree::Font>& font) override;

  float GetTextWidth(const char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     render_tree::FontProvider* font_provider,
                     render_tree::FontVector* maybe_used_fonts) override;

  scoped_refptr<render_tree::Mesh> CreateMesh(
      scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
      render_tree::Mesh::DrawMode draw_mode) override;

  scoped_refptr<render_tree::Image> DrawOffscreenImage(
      const scoped_refptr<render_tree::Node>& root) override;

 private:
  backend::GraphicsContextEGL* cobalt_context_;
  GrContext* gr_context_;
  SubmitOffscreenCallback submit_offscreen_callback_;
  const bool purge_skia_font_caches_on_destruction_;

  TextShaper text_shaper_;
  int max_texture_size_;

#if SB_HAS(GRAPHICS)
  static void GraphicsContextRunner(
      SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context);

  SbDecodeTargetGraphicsContextProvider
      decode_target_graphics_context_provider_;
#endif  // SB_HAS(GRAPHICS)

  // We keep a handle to the message loop that this resource provider was
  // created on.  This message loop is used whenever we need to issue graphics
  // commands, such as during the creation of an image.  If the message loop
  // is NULL, we will try to issue the graphics commands from the current
  // thread.
  MessageLoop* self_message_loop_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RESOURCE_PROVIDER_H_
