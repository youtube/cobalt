// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_RESOURCE_PROVIDER_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_RESOURCE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/rasterizer/blitter/image.h"
#include "starboard/blitter.h"
#include "starboard/decode_target.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

class ResourceProvider : public render_tree::ResourceProvider {
 public:
  ResourceProvider(SbBlitterDevice device,
                   render_tree::ResourceProvider* skia_resource_provider,
                   SubmitOffscreenCallback submit_offscreen_callback);
  ~ResourceProvider() override {}

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<ResourceProvider>();
  }

  void Finish() override {}

  scoped_refptr<render_tree::Image> CreateImageFromSbDecodeTarget(
      SbDecodeTarget decode_target) override;

  bool SupportsSbDecodeTarget() override { return true; }

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
    return &decode_target_graphics_context_provider_;
  }

  bool PixelFormatSupported(render_tree::PixelFormat pixel_format) override;
  bool AlphaFormatSupported(render_tree::AlphaFormat alpha_format) override;

  scoped_ptr<render_tree::ImageData> AllocateImageData(
      const math::Size& size, render_tree::PixelFormat pixel_format,
      render_tree::AlphaFormat alpha_format) override;

  scoped_refptr<render_tree::Image> CreateImage(
      scoped_ptr<render_tree::ImageData> source_data) override;

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
      int32 utf32_character, render_tree::FontStyle font_style,
      const std::string& language) override;

  scoped_refptr<render_tree::Typeface> CreateTypefaceFromRawData(
      scoped_ptr<RawTypefaceDataVector> raw_data,
      std::string* error_string) override;

  float GetTextWidth(const char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     render_tree::FontProvider* font_provider,
                     render_tree::FontVector* maybe_used_fonts) override;

  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      render_tree::FontProvider* font_provider) override;

  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const std::string& utf8_string,
      const scoped_refptr<render_tree::Font>& font) override;

  scoped_refptr<render_tree::Mesh> CreateMesh(
      scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
      render_tree::Mesh::DrawMode draw_mode) override;

  scoped_refptr<render_tree::Image> DrawOffscreenImage(
      const scoped_refptr<render_tree::Node>& root) override;

 private:
  SbBlitterDevice device_;

  // Any requests for font-related resources will be forwarded on to this
  // ResourceProvider, as the Blitter API does not natively support font
  // rendering.
  render_tree::ResourceProvider* skia_resource_provider_;

  SubmitOffscreenCallback submit_offscreen_callback_;

  SbDecodeTargetGraphicsContextProvider
      decode_target_graphics_context_provider_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_RESOURCE_PROVIDER_H_
