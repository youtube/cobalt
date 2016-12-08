/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RESOURCE_PROVIDER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RESOURCE_PROVIDER_H_

#include <string>

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
                           GrContext* gr_context);

  void Finish() OVERRIDE;

  bool PixelFormatSupported(render_tree::PixelFormat pixel_format) OVERRIDE;
  bool AlphaFormatSupported(render_tree::AlphaFormat alpha_format) OVERRIDE;

  scoped_ptr<render_tree::ImageData> AllocateImageData(
      const math::Size& size, render_tree::PixelFormat pixel_format,
      render_tree::AlphaFormat alpha_format) OVERRIDE;

  scoped_refptr<render_tree::Image> CreateImage(
      scoped_ptr<render_tree::ImageData> pixel_data) OVERRIDE;

  scoped_ptr<render_tree::RawImageMemory> AllocateRawImageMemory(
      size_t size_in_bytes, size_t alignment) OVERRIDE;

  scoped_refptr<render_tree::Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
      const render_tree::MultiPlaneImageDataDescriptor& descriptor) OVERRIDE;

  bool HasLocalFontFamily(const char* font_family_name) const OVERRIDE;

  scoped_refptr<render_tree::Typeface> GetLocalTypeface(
      const char* font_family_name, render_tree::FontStyle font_style) OVERRIDE;

  scoped_refptr<render_tree::Typeface> GetCharacterFallbackTypeface(
      int32 character, render_tree::FontStyle font_style,
      const std::string& language) OVERRIDE;

  // This resource provider uses ots (OpenTypeSanitizer) to sanitize the raw
  // typeface data and skia to generate the typeface. It supports TrueType,
  // OpenType, and WOFF data formats.
  scoped_refptr<render_tree::Typeface> CreateTypefaceFromRawData(
      scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data,
      std::string* error_string) OVERRIDE;

  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      render_tree::FontProvider* font_provider) OVERRIDE;

  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const std::string& utf8_string,
      const scoped_refptr<render_tree::Font>& font) OVERRIDE;

  float GetTextWidth(const char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     render_tree::FontProvider* font_provider,
                     render_tree::FontVector* maybe_used_fonts) OVERRIDE;

 private:
  backend::GraphicsContextEGL* cobalt_context_;
  GrContext* gr_context_;

  SkAutoTUnref<SkFontMgr> font_manager_;
  TextShaper text_shaper_;

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
