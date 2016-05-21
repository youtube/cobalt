/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/resource_provider.h"

#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/renderer/rasterizer/blitter/image.h"
#include "cobalt/renderer/rasterizer/blitter/render_tree_blitter_conversions.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

using render_tree::AlphaFormat;
using render_tree::Font;
using render_tree::FontProvider;
using render_tree::FontStyle;
using render_tree::FontVector;
using render_tree::GlyphBuffer;
using render_tree::PixelFormat;
using render_tree::Typeface;

ResourceProvider::ResourceProvider(SbBlitterDevice device) { device_ = device; }

bool ResourceProvider::PixelFormatSupported(PixelFormat pixel_format) {
  return SbBlitterIsPixelFormatSupportedByPixelData(
      device_, RenderTreePixelFormatToBlitter(pixel_format),
      kSbBlitterAlphaFormatPremultiplied);
}

scoped_ptr<render_tree::ImageData> ResourceProvider::AllocateImageData(
    const math::Size& size, PixelFormat pixel_format,
    AlphaFormat alpha_format) {
  return scoped_ptr<render_tree::ImageData>(
      new ImageData(device_, size, pixel_format, alpha_format));
}

scoped_refptr<render_tree::Image> ResourceProvider::CreateImage(
    scoped_ptr<render_tree::ImageData> source_data) {
  scoped_ptr<ImageData> blitter_source_data(
      base::polymorphic_downcast<ImageData*>(source_data.release()));
  return make_scoped_refptr(new Image(blitter_source_data.Pass()));
}

scoped_ptr<render_tree::RawImageMemory>
ResourceProvider::AllocateRawImageMemory(size_t size_in_bytes,
                                         size_t alignment) {
  return scoped_ptr<render_tree::RawImageMemory>(
      new render_tree::RawImageMemoryStub(size_in_bytes, alignment));
}

scoped_refptr<render_tree::Image>
ResourceProvider::CreateMultiPlaneImageFromRawMemory(
    scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  UNREFERENCED_PARAMETER(raw_image_memory);
  UNREFERENCED_PARAMETER(descriptor);
  return scoped_refptr<render_tree::Image>();
}

bool ResourceProvider::HasLocalFontFamily(const char* font_family_name) const {
  UNREFERENCED_PARAMETER(font_family_name);
  return true;
}

scoped_refptr<Typeface> ResourceProvider::GetLocalTypeface(
    const char* font_family_name, FontStyle font_style) {
  UNREFERENCED_PARAMETER(font_family_name);
  UNREFERENCED_PARAMETER(font_style);
  return make_scoped_refptr(new render_tree::TypefaceStub(NULL));
}

scoped_refptr<Typeface> ResourceProvider::GetCharacterFallbackTypeface(
    int32 utf32_character, FontStyle font_style, const std::string& language) {
  UNREFERENCED_PARAMETER(utf32_character);
  UNREFERENCED_PARAMETER(font_style);
  UNREFERENCED_PARAMETER(language);
  return make_scoped_refptr(new render_tree::TypefaceStub(NULL));
}

scoped_refptr<Typeface> ResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<RawTypefaceDataVector> raw_data, std::string* error_string) {
  UNREFERENCED_PARAMETER(raw_data);
  UNREFERENCED_PARAMETER(error_string);
  return make_scoped_refptr(new render_tree::TypefaceStub(NULL));
}

float ResourceProvider::GetTextWidth(const char16* text_buffer,
                                     size_t text_length,
                                     const std::string& language, bool is_rtl,
                                     FontProvider* font_provider,
                                     FontVector* maybe_used_fonts) {
  UNREFERENCED_PARAMETER(text_buffer);
  UNREFERENCED_PARAMETER(language);
  UNREFERENCED_PARAMETER(is_rtl);
  UNREFERENCED_PARAMETER(font_provider);
  UNREFERENCED_PARAMETER(maybe_used_fonts);
  return static_cast<float>(text_length);
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, FontProvider* font_provider) {
  UNREFERENCED_PARAMETER(text_buffer);
  UNREFERENCED_PARAMETER(language);
  UNREFERENCED_PARAMETER(is_rtl);
  UNREFERENCED_PARAMETER(font_provider);
  return make_scoped_refptr(
      new GlyphBuffer(math::RectF(0, 0, static_cast<float>(text_length), 1)));
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string, const scoped_refptr<Font>& font) {
  UNREFERENCED_PARAMETER(font);
  return make_scoped_refptr(new GlyphBuffer(
      math::RectF(0, 0, static_cast<float>(utf8_string.size()), 1)));
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
