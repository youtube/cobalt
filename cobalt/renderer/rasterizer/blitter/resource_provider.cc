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

ResourceProvider::ResourceProvider(
    SbBlitterDevice device,
    render_tree::ResourceProvider* font_resource_provider)
    : device_(device), font_resource_provider_(font_resource_provider) {}

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
  return font_resource_provider_->HasLocalFontFamily(font_family_name);
}

scoped_refptr<Typeface> ResourceProvider::GetLocalTypeface(
    const char* font_family_name, FontStyle font_style) {
  return font_resource_provider_->GetLocalTypeface(font_family_name,
                                                   font_style);
}

scoped_refptr<Typeface> ResourceProvider::GetCharacterFallbackTypeface(
    int32 utf32_character, FontStyle font_style, const std::string& language) {
  return font_resource_provider_->GetCharacterFallbackTypeface(
      utf32_character, font_style, language);
}

scoped_refptr<Typeface> ResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<RawTypefaceDataVector> raw_data, std::string* error_string) {
  return font_resource_provider_->CreateTypefaceFromRawData(raw_data.Pass(),
                                                            error_string);
}

float ResourceProvider::GetTextWidth(const char16* text_buffer,
                                     size_t text_length,
                                     const std::string& language, bool is_rtl,
                                     FontProvider* font_provider,
                                     FontVector* maybe_used_fonts) {
  return font_resource_provider_->GetTextWidth(text_buffer, text_length,
                                               language, is_rtl, font_provider,
                                               maybe_used_fonts);
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, FontProvider* font_provider) {
  return font_resource_provider_->CreateGlyphBuffer(
      text_buffer, text_length, language, is_rtl, font_provider);
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string, const scoped_refptr<Font>& font) {
  return font_resource_provider_->CreateGlyphBuffer(utf8_string, font);
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
