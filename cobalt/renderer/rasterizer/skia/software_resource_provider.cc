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

#include "cobalt/renderer/rasterizer/skia/software_resource_provider.h"

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/font.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/software_image.h"
#include "cobalt/renderer/rasterizer/skia/typeface.h"
#include "third_party/ots/include/opentype-sanitiser.h"
#include "third_party/ots/include/ots-memory-stream.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontMgr.h"

using cobalt::render_tree::ImageData;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

bool SkiaSoftwareResourceProvider::PixelFormatSupported(
    render_tree::PixelFormat pixel_format) {
  return RenderTreeSurfaceFormatToSkia(pixel_format) == kN32_SkColorType;
}

scoped_ptr<ImageData> SkiaSoftwareResourceProvider::AllocateImageData(
    const math::Size& size, render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::AllocateImageData()");
  DCHECK(PixelFormatSupported(pixel_format));
  return scoped_ptr<ImageData>(
      new SkiaSoftwareImageData(size, pixel_format, alpha_format));
}

scoped_refptr<render_tree::Image> SkiaSoftwareResourceProvider::CreateImage(
    scoped_ptr<ImageData> source_data) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::CreateImage()");
  scoped_ptr<SkiaSoftwareImageData> skia_source_data(
      base::polymorphic_downcast<SkiaSoftwareImageData*>(
          source_data.release()));

  return scoped_refptr<render_tree::Image>(
      new SkiaSoftwareImage(skia_source_data.Pass()));
}

scoped_ptr<render_tree::RawImageMemory>
SkiaSoftwareResourceProvider::AllocateRawImageMemory(size_t size_in_bytes,
                                                     size_t alignment) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::AllocateRawImageMemory()");

  return scoped_ptr<render_tree::RawImageMemory>(
      new SkiaSoftwareRawImageMemory(size_in_bytes, alignment));
}

scoped_refptr<render_tree::Image>
SkiaSoftwareResourceProvider::CreateMultiPlaneImageFromRawMemory(
    scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  TRACE_EVENT0(
      "cobalt::renderer",
      "SkiaSoftwareResourceProvider::CreateMultiPlaneImageFromRawMemory()");

  scoped_ptr<SkiaSoftwareRawImageMemory> skia_software_raw_image_memory(
      base::polymorphic_downcast<SkiaSoftwareRawImageMemory*>(
          raw_image_memory.release()));

  return make_scoped_refptr(new SkiaSoftwareMultiPlaneImage(
      skia_software_raw_image_memory.Pass(), descriptor));
}

bool SkiaSoftwareResourceProvider::HasLocalFontFamily(
    const char* font_family_name) const {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::HasLocalFontFamily()");

  SkAutoTUnref<SkFontMgr> fm(SkFontMgr::RefDefault());
  SkAutoTUnref<SkFontStyleSet> style_set(fm->matchFamily(font_family_name));
  return style_set->count() > 0;
}

scoped_refptr<render_tree::Typeface>
SkiaSoftwareResourceProvider::GetLocalTypeface(
    const char* font_family_name, render_tree::FontStyle font_style) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::GetLocalFont()");

  SkAutoTUnref<SkFontMgr> fm(SkFontMgr::RefDefault());
  SkAutoTUnref<SkTypeface> typeface(fm->matchFamilyStyle(
      font_family_name, CobaltFontStyleToSkFontStyle(font_style)));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SkiaSoftwareResourceProvider::GetCharacterFallbackTypeface(
    int32 character, render_tree::FontStyle font_style,
    const std::string& language) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::GetCharacterFallbackTypeface()");

  SkAutoTUnref<SkFontMgr> fm(SkFontMgr::RefDefault());
  SkAutoTUnref<SkTypeface> typeface(
      fm->matchFamilyStyleCharacter(0, CobaltFontStyleToSkFontStyle(font_style),
                                    language.c_str(), character));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SkiaSoftwareResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data,
    std::string* error_string) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareResourceProvider::CreateFontFromRawData()");

  if (raw_data == NULL) {
    *error_string = "No data to process";
    return NULL;
  }

  ots::ExpandingMemoryStream sanitized_data(
      raw_data->size(), render_tree::ResourceProvider::kMaxTypefaceDataSize);
  if (!ots::Process(&sanitized_data, &((*raw_data)[0]), raw_data->size())) {
    *error_string = "OpenType sanitizer unable to process data";
    return NULL;
  }

  // Free the raw data now that we're done with it.
  raw_data.reset();

  SkAutoTUnref<SkData> skia_data(SkData::NewWithCopy(
      sanitized_data.get(), static_cast<size_t>(sanitized_data.Tell())));

  SkAutoTUnref<SkStream> stream(new SkMemoryStream(skia_data));
  SkAutoTUnref<SkTypeface> typeface(SkTypeface::CreateFromStream(stream));
  if (typeface) {
    return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
  } else {
    *error_string = "Skia unable to create typeface";
    return NULL;
  }
}

scoped_refptr<render_tree::GlyphBuffer>
SkiaSoftwareResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider) {
  return text_shaper_.CreateGlyphBuffer(text_buffer, text_length, language,
                                        is_rtl, font_provider);
}

scoped_refptr<render_tree::GlyphBuffer>
SkiaSoftwareResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string,
    const scoped_refptr<render_tree::Font>& font) {
  return text_shaper_.CreateGlyphBuffer(utf8_string, font);
}

float SkiaSoftwareResourceProvider::GetTextWidth(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider,
    render_tree::FontVector* maybe_used_fonts) {
  return text_shaper_.GetTextWidth(text_buffer, text_length, language, is_rtl,
                                   font_provider, maybe_used_fonts);
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
