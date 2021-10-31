// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/renderer/rasterizer/blitter/resource_provider.h"

#include "base/bind.h"
#include "cobalt/renderer/rasterizer/blitter/render_tree_blitter_conversions.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

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
    render_tree::ResourceProvider* skia_resource_provider,
    SubmitOffscreenCallback submit_offscreen_callback)
    : device_(device),
      skia_resource_provider_(skia_resource_provider),
      submit_offscreen_callback_(submit_offscreen_callback) {
  decode_target_graphics_context_provider_.device = device;
}

bool ResourceProvider::PixelFormatSupported(PixelFormat pixel_format) {
  if (pixel_format == render_tree::kPixelFormatUYVY) {
    return false;
  }

  return SbBlitterIsPixelFormatSupportedByPixelData(
      device_, RenderTreePixelFormatToBlitter(pixel_format));
}

bool ResourceProvider::AlphaFormatSupported(
    render_tree::AlphaFormat alpha_format) {
  return alpha_format == render_tree::kAlphaFormatPremultiplied ||
         alpha_format == render_tree::kAlphaFormatOpaque;
}

std::unique_ptr<render_tree::ImageData> ResourceProvider::AllocateImageData(
    const math::Size& size, PixelFormat pixel_format,
    AlphaFormat alpha_format) {
  DCHECK(PixelFormatSupported(pixel_format));
  DCHECK(AlphaFormatSupported(alpha_format));

  std::unique_ptr<render_tree::ImageData> image_data(
      new ImageData(device_, size, pixel_format, alpha_format));
  if (!image_data->GetMemory()) {
    return std::unique_ptr<render_tree::ImageData>();
  } else {
    return std::move(image_data);
  }
}

scoped_refptr<render_tree::Image> ResourceProvider::CreateImage(
    std::unique_ptr<render_tree::ImageData> source_data) {
  std::unique_ptr<ImageData> blitter_source_data(
      base::polymorphic_downcast<ImageData*>(source_data.release()));
  return base::WrapRefCounted(
      new SinglePlaneImage(std::move(blitter_source_data)));
}

scoped_refptr<render_tree::Image>
ResourceProvider::CreateImageFromSbDecodeTarget(SbDecodeTarget decode_target) {
  SbDecodeTargetInfo info;
  memset(&info, 0, sizeof(info));
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
    return base::WrapRefCounted(new SinglePlaneImage(
        plane.surface, info.is_opaque,
        base::Bind(&SbDecodeTargetRelease, decode_target)));
  }

  NOTREACHED()
      << "Only format kSbDecodeTargetFormat1PlaneRGBA is currently supported.";
  SbDecodeTargetRelease(decode_target);
  return NULL;
}

std::unique_ptr<render_tree::RawImageMemory>
ResourceProvider::AllocateRawImageMemory(size_t size_in_bytes,
                                         size_t alignment) {
  return skia_resource_provider_->AllocateRawImageMemory(size_in_bytes,
                                                         alignment);
}

scoped_refptr<render_tree::Image>
ResourceProvider::CreateMultiPlaneImageFromRawMemory(
    std::unique_ptr<render_tree::RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  return skia_resource_provider_->CreateMultiPlaneImageFromRawMemory(
      std::move(raw_image_memory), descriptor);
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
    const char* font_face_name) {
  return skia_resource_provider_->GetLocalTypefaceByFaceNameIfAvailable(
      font_face_name);
}

scoped_refptr<Typeface> ResourceProvider::GetCharacterFallbackTypeface(
    int32 utf32_character, FontStyle font_style, const std::string& language) {
  return skia_resource_provider_->GetCharacterFallbackTypeface(
      utf32_character, font_style, language);
}

void ResourceProvider::LoadAdditionalFonts() {
  return skia_resource_provider_->LoadAdditionalFonts();
}

scoped_refptr<Typeface> ResourceProvider::CreateTypefaceFromRawData(
    std::unique_ptr<RawTypefaceDataVector> raw_data,
    std::string* error_string) {
  return skia_resource_provider_->CreateTypefaceFromRawData(std::move(raw_data),
                                                            error_string);
}

float ResourceProvider::GetTextWidth(const base::char16* text_buffer,
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
    const base::char16* text_buffer, size_t text_length,
    const std::string& language, bool is_rtl, FontProvider* font_provider) {
  return skia_resource_provider_->CreateGlyphBuffer(
      text_buffer, text_length, language, is_rtl, font_provider);
}

// Creates a glyph buffer, which is populated with shaped text, and used to
// render that text.
scoped_refptr<GlyphBuffer> ResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string, const scoped_refptr<Font>& font) {
  return skia_resource_provider_->CreateGlyphBuffer(utf8_string, font);
}

scoped_refptr<render_tree::LottieAnimation>
ResourceProvider::CreateLottieAnimation(const char* data, size_t length) {
  NOTIMPLEMENTED();
  return scoped_refptr<render_tree::LottieAnimation>(NULL);
}

scoped_refptr<render_tree::Mesh> ResourceProvider::CreateMesh(
    std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
    render_tree::Mesh::DrawMode draw_mode) {
  NOTIMPLEMENTED();
  return scoped_refptr<render_tree::Mesh>(NULL);
}

scoped_refptr<render_tree::Image> ResourceProvider::DrawOffscreenImage(
    const scoped_refptr<render_tree::Node>& root) {
  return base::WrapRefCounted(
      new SinglePlaneImage(root, submit_offscreen_callback_, device_));
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)
