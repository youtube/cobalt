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

#include "cobalt/renderer/rasterizer/blitter/resource_provider.h"

#include "base/bind.h"
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
    render_tree::ResourceProvider* skia_resource_provider)
    : device_(device), skia_resource_provider_(skia_resource_provider) {
#if SB_API_VERSION >= 4
  decode_target_graphics_context_provider_.device = device;
#endif  // SB_API_VERSION >= 4
}

bool ResourceProvider::PixelFormatSupported(PixelFormat pixel_format) {
  return SbBlitterIsPixelFormatSupportedByPixelData(
      device_, RenderTreePixelFormatToBlitter(pixel_format));
}

bool ResourceProvider::AlphaFormatSupported(
    render_tree::AlphaFormat alpha_format) {
  return alpha_format == render_tree::kAlphaFormatPremultiplied ||
         alpha_format == render_tree::kAlphaFormatOpaque;
}

scoped_ptr<render_tree::ImageData> ResourceProvider::AllocateImageData(
    const math::Size& size, PixelFormat pixel_format,
    AlphaFormat alpha_format) {
  DCHECK(PixelFormatSupported(pixel_format));
  DCHECK(AlphaFormatSupported(alpha_format));

  scoped_ptr<render_tree::ImageData> image_data(
      new ImageData(device_, size, pixel_format, alpha_format));
  if (!image_data->GetMemory()) {
    return scoped_ptr<render_tree::ImageData>();
  } else {
    return image_data.Pass();
  }
}

scoped_refptr<render_tree::Image> ResourceProvider::CreateImage(
    scoped_ptr<render_tree::ImageData> source_data) {
  scoped_ptr<ImageData> blitter_source_data(
      base::polymorphic_downcast<ImageData*>(source_data.release()));
  return make_scoped_refptr(new SinglePlaneImage(blitter_source_data.Pass()));
}

#if SB_API_VERSION >= 3 && SB_HAS(GRAPHICS)

scoped_refptr<render_tree::Image>
ResourceProvider::CreateImageFromSbDecodeTarget(SbDecodeTarget decode_target) {
#if SB_API_VERSION < 4
  SbDecodeTargetFormat format = SbDecodeTargetGetFormat(decode_target);
  if (format == kSbDecodeTargetFormat1PlaneRGBA) {
    SbBlitterSurface surface =
        SbDecodeTargetGetPlane(decode_target, kSbDecodeTargetPlaneRGBA);
    DCHECK(SbBlitterIsSurfaceValid(surface));
    bool is_opaque = SbDecodeTargetIsOpaque(decode_target);

    // Now that we have the surface it contained, we are free to delete
    // |decode_target|.
    SbDecodeTargetDestroy(decode_target);
    return make_scoped_refptr(
        new SinglePlaneImage(surface, is_opaque, base::Closure()));
#else   // SB_API_VERSION < 4
  SbDecodeTargetInfo info;
  SbMemorySet(&info, 0, sizeof(info));
  CHECK(SbDecodeTargetGetInfo(decode_target, &info));

  SbDecodeTargetFormat format = info.format;
  if (format == kSbDecodeTargetFormat1PlaneRGBA) {
    const SbDecodeTargetInfoPlane& plane =
        info.planes[kSbDecodeTargetPlaneRGBA];
    DCHECK(SbBlitterIsSurfaceValid(plane.surface));
    if (plane.content_region.left != 0 || plane.content_region.top != 0 ||
        plane.content_region.right != plane.width ||
        plane.content_region.bottom != plane.height) {
      NOTREACHED() << "Cobalt has not yet implemented support for Blitter "
                      "decode target content regions.";
    }
    return make_scoped_refptr(new SinglePlaneImage(
        plane.surface, info.is_opaque,
        base::Bind(&SbDecodeTargetRelease, decode_target)));
#endif  // SB_API_VERSION < 4
  }

  NOTREACHED()
      << "Only format kSbDecodeTargetFormat1PlaneRGBA is currently supported.";
#if SB_API_VERSION < 4
  SbDecodeTargetDestroy(decode_target);
#else   // SB_API_VERSION < 4
  SbDecodeTargetRelease(decode_target);
#endif  // SB_API_VERSION < 4
  return NULL;
}

#endif  // SB_API_VERSION >= 3 && SB_HAS(GRAPHICS)

scoped_ptr<render_tree::RawImageMemory>
ResourceProvider::AllocateRawImageMemory(size_t size_in_bytes,
                                         size_t alignment) {
  return skia_resource_provider_->AllocateRawImageMemory(size_in_bytes,
                                                         alignment);
}

scoped_refptr<render_tree::Image>
ResourceProvider::CreateMultiPlaneImageFromRawMemory(
    scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  return skia_resource_provider_->CreateMultiPlaneImageFromRawMemory(
      raw_image_memory.Pass(), descriptor);
}

bool ResourceProvider::HasLocalFontFamily(const char* font_family_name) const {
  return skia_resource_provider_->HasLocalFontFamily(font_family_name);
}

scoped_refptr<Typeface> ResourceProvider::GetLocalTypeface(
    const char* font_family_name, FontStyle font_style) {
  return skia_resource_provider_->GetLocalTypeface(font_family_name,
                                                   font_style);
}

scoped_refptr<render_tree::Typeface>
ResourceProvider::GetLocalTypefaceByFaceNameIfAvailable(
    const std::string& font_face_name) {
  return skia_resource_provider_->GetLocalTypefaceByFaceNameIfAvailable(
      font_face_name);
}

scoped_refptr<Typeface> ResourceProvider::GetCharacterFallbackTypeface(
    int32 utf32_character, FontStyle font_style, const std::string& language) {
  return skia_resource_provider_->GetCharacterFallbackTypeface(
      utf32_character, font_style, language);
}

scoped_refptr<Typeface> ResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<RawTypefaceDataVector> raw_data, std::string* error_string) {
  return skia_resource_provider_->CreateTypefaceFromRawData(raw_data.Pass(),
                                                            error_string);
}

float ResourceProvider::GetTextWidth(const char16* text_buffer,
                                     size_t text_length,
                                     const std::string& language, bool is_rtl,
                                     FontProvider* font_provider,
                                     FontVector* maybe_used_fonts) {
  return skia_resource_provider_->GetTextWidth(text_buffer, text_length,
                                               language, is_rtl, font_provider,
                                               maybe_used_fonts);
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, FontProvider* font_provider) {
  return skia_resource_provider_->CreateGlyphBuffer(
      text_buffer, text_length, language, is_rtl, font_provider);
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string, const scoped_refptr<Font>& font) {
  return skia_resource_provider_->CreateGlyphBuffer(utf8_string, font);
}

scoped_refptr<render_tree::Mesh> ResourceProvider::CreateMesh(
    scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
    render_tree::Mesh::DrawMode draw_mode) {
  NOTIMPLEMENTED();
  return scoped_refptr<render_tree::Mesh>(NULL);
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
