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

#include "cobalt/renderer/rasterizer/skia/software_resource_provider.h"

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/font.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/software_image.h"
#include "cobalt/renderer/rasterizer/skia/software_mesh.h"
#include "cobalt/renderer/rasterizer/skia/typeface.h"
#include "third_party/ots/include/opentype-sanitiser.h"
#include "third_party/ots/include/ots-memory-stream.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

using cobalt::render_tree::ImageData;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SoftwareResourceProvider::SoftwareResourceProvider(
    bool purge_skia_font_caches_on_destruction)
    : purge_skia_font_caches_on_destruction_(
          purge_skia_font_caches_on_destruction) {
  // Initialize the font manager now to ensure that it doesn't get initialized
  // on multiple threads simultaneously later.
  SkFontMgr::RefDefault();
}

SoftwareResourceProvider::~SoftwareResourceProvider() {
  if (purge_skia_font_caches_on_destruction_) {
    text_shaper_.PurgeCaches();

    sk_sp<SkFontMgr> font_manager(SkFontMgr::RefDefault());
    SkFontMgr_Cobalt* cobalt_font_manager =
        base::polymorphic_downcast<SkFontMgr_Cobalt*>(font_manager.get());
    cobalt_font_manager->PurgeCaches();
  }
}

bool SoftwareResourceProvider::PixelFormatSupported(
    render_tree::PixelFormat pixel_format) {
  return RenderTreeSurfaceFormatToSkia(pixel_format) == kN32_SkColorType;
}

bool SoftwareResourceProvider::AlphaFormatSupported(
    render_tree::AlphaFormat alpha_format) {
  return alpha_format == render_tree::kAlphaFormatPremultiplied ||
         alpha_format == render_tree::kAlphaFormatOpaque;
}

scoped_ptr<ImageData> SoftwareResourceProvider::AllocateImageData(
    const math::Size& size, render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format) {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::AllocateImageData()");
  DCHECK(PixelFormatSupported(pixel_format));
  DCHECK(AlphaFormatSupported(alpha_format));
  return scoped_ptr<ImageData>(
      new SoftwareImageData(size, pixel_format, alpha_format));
}

scoped_refptr<render_tree::Image> SoftwareResourceProvider::CreateImage(
    scoped_ptr<ImageData> source_data) {
  TRACE_EVENT0("cobalt::renderer", "SoftwareResourceProvider::CreateImage()");
  scoped_ptr<SoftwareImageData> skia_source_data(
      base::polymorphic_downcast<SoftwareImageData*>(source_data.release()));

  return scoped_refptr<render_tree::Image>(
      new SoftwareImage(skia_source_data.Pass()));
}

scoped_ptr<render_tree::RawImageMemory>
SoftwareResourceProvider::AllocateRawImageMemory(size_t size_in_bytes,
                                                 size_t alignment) {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::AllocateRawImageMemory()");

  return scoped_ptr<render_tree::RawImageMemory>(
      new SoftwareRawImageMemory(size_in_bytes, alignment));
}

scoped_refptr<render_tree::Image>
SoftwareResourceProvider::CreateMultiPlaneImageFromRawMemory(
    scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  TRACE_EVENT0(
      "cobalt::renderer",
      "SoftwareResourceProvider::CreateMultiPlaneImageFromRawMemory()");

  scoped_ptr<SoftwareRawImageMemory> skia_software_raw_image_memory(
      base::polymorphic_downcast<SoftwareRawImageMemory*>(
          raw_image_memory.release()));

  return make_scoped_refptr(new SoftwareMultiPlaneImage(
      skia_software_raw_image_memory.Pass(), descriptor));
}

bool SoftwareResourceProvider::HasLocalFontFamily(
    const char* font_family_name) const {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::HasLocalFontFamily()");

  sk_sp<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  sk_sp<SkFontStyleSet> style_set(
      font_manager->matchFamily(font_family_name));
  return style_set->count() > 0;
}

scoped_refptr<render_tree::Typeface> SoftwareResourceProvider::GetLocalTypeface(
    const char* font_family_name, render_tree::FontStyle font_style) {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::GetLocalTypeface()");

  sk_sp<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  sk_sp<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          font_manager->matchFamilyStyle(
              font_family_name, CobaltFontStyleToSkFontStyle(font_style))));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SoftwareResourceProvider::GetLocalTypefaceByFaceNameIfAvailable(
    const char* font_face_name) {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::GetLocalTypefaceIfAvailable()");

  sk_sp<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  SkFontMgr_Cobalt* cobalt_font_manager =
      base::polymorphic_downcast<SkFontMgr_Cobalt*>(font_manager.get());

  sk_sp<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          cobalt_font_manager->MatchFaceName(font_face_name)));

  if (!typeface) {
    return nullptr;
  }

  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SoftwareResourceProvider::GetCharacterFallbackTypeface(
    int32 character, render_tree::FontStyle font_style,
    const std::string& language) {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::GetCharacterFallbackTypeface()");

  sk_sp<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  const char* language_cstr = language.c_str();
  sk_sp<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          font_manager->matchFamilyStyleCharacter(
              NULL, CobaltFontStyleToSkFontStyle(font_style), &language_cstr, 1,
              character)));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SoftwareResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data,
    std::string* error_string) {
  TRACE_EVENT0("cobalt::renderer",
               "SoftwareResourceProvider::CreateFontFromRawData()");

  if (raw_data == NULL) {
    *error_string = "No data to process";
    return NULL;
  }

  ots::ExpandingMemoryStream sanitized_data(
      raw_data->size(), render_tree::ResourceProvider::kMaxTypefaceDataSize);
  ots::OTSContext context;
  if (!context.Process(&sanitized_data, &((*raw_data)[0]), raw_data->size())) {
    *error_string = "OpenType sanitizer unable to process data";
    return NULL;
  }

  // Free the raw data now that we're done with it.
  raw_data.reset();

  scoped_ptr<SkStreamAsset> stream;
  {
    sk_sp<SkData> skia_data(SkData::MakeWithCopy(
        sanitized_data.get(), static_cast<size_t>(sanitized_data.Tell())));
    stream.reset(new SkMemoryStream(skia_data));
  }

  sk_sp<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          SkTypeface::MakeFromStream(stream.release()).release()));
  if (typeface) {
    return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
  } else {
    *error_string = "Skia unable to create typeface";
    return NULL;
  }
}

scoped_refptr<render_tree::GlyphBuffer>
SoftwareResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider) {
  return text_shaper_.CreateGlyphBuffer(text_buffer, text_length, language,
                                        is_rtl, font_provider);
}

scoped_refptr<render_tree::GlyphBuffer>
SoftwareResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string,
    const scoped_refptr<render_tree::Font>& font) {
  return text_shaper_.CreateGlyphBuffer(utf8_string, font);
}

float SoftwareResourceProvider::GetTextWidth(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider,
    render_tree::FontVector* maybe_used_fonts) {
  return text_shaper_.GetTextWidth(text_buffer, text_length, language, is_rtl,
                                   font_provider, maybe_used_fonts);
}

scoped_refptr<render_tree::Mesh> SoftwareResourceProvider::CreateMesh(
    scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
    render_tree::Mesh::DrawMode draw_mode) {
  return new SoftwareMesh(vertices.Pass(), draw_mode);
}

scoped_refptr<render_tree::Image> SoftwareResourceProvider::DrawOffscreenImage(
    const scoped_refptr<render_tree::Node>& root) {
  UNREFERENCED_PARAMETER(root);
  return scoped_refptr<render_tree::Image>(NULL);
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
