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

#include "cobalt/renderer/rasterizer/skia/hardware_resource_provider.h"

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/font.h"
#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/hardware_image.h"
#include "cobalt/renderer/rasterizer/skia/typeface.h"
#include "third_party/ots/include/opentype-sanitiser.h"
#include "third_party/ots/include/ots-memory-stream.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontMgr.h"

using cobalt::render_tree::Image;
using cobalt::render_tree::ImageData;
using cobalt::render_tree::RawImageMemory;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkiaHardwareResourceProvider::SkiaHardwareResourceProvider(
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context)
    : cobalt_context_(cobalt_context),
      gr_context_(gr_context),
      self_message_loop_(MessageLoop::current()) {}

scoped_ptr<ImageData> SkiaHardwareResourceProvider::AllocateImageData(
    const math::Size& size, render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::AllocateImageData()");
  DCHECK_EQ(render_tree::kPixelFormatRGBA8, pixel_format)
      << "Currently, only RGBA8 is supported.";

  return scoped_ptr<ImageData>(new SkiaHardwareImageData(
      cobalt_context_->system_egl()->AllocateTextureData(
          size, ConvertRenderTreeFormatToGL(pixel_format)),
      pixel_format, alpha_format));
}

scoped_refptr<Image> SkiaHardwareResourceProvider::CreateImage(
    scoped_ptr<ImageData> source_data) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::CreateImage()");
  scoped_ptr<SkiaHardwareImageData> skia_hardware_source_data(
      base::polymorphic_downcast<SkiaHardwareImageData*>(
          source_data.release()));
  const render_tree::ImageDataDescriptor& descriptor =
      skia_hardware_source_data->GetDescriptor();

  if (descriptor.alpha_format != render_tree::kAlphaFormatPremultiplied) {
    SkiaImage::SkiaConvertImageData(
        descriptor.size, descriptor.pitch_in_bytes,
        RenderTreeSurfaceFormatToSkia(descriptor.pixel_format),
        RenderTreeAlphaFormatToSkia(descriptor.alpha_format),
        skia_hardware_source_data->GetMemory(), descriptor.pitch_in_bytes,
        skia_hardware_source_data->GetMemory(),
        RenderTreeSurfaceFormatToSkia(descriptor.pixel_format),
        kPremul_SkAlphaType);
  } else {
    DCHECK_EQ(render_tree::kAlphaFormatPremultiplied, descriptor.alpha_format);
#if defined(COBALT_BUILD_TYPE_DEBUG)
    SkiaImage::DCheckForPremultipliedAlpha(
        descriptor.size, descriptor.pitch_in_bytes, descriptor.pixel_format,
        skia_hardware_source_data->GetMemory());
#endif
  }

  // Construct a frontend image from this data, which internally will send
  // a message to the rasterizer thread passing along the image data where the
  // backend texture will be constructed, and associated with this frontend
  // texture through a map that will be accessed when the rasterizer visits
  // any subsequently submitted render trees referencing the frontend image.
  return make_scoped_refptr(new SkiaHardwareFrontendImage(
      skia_hardware_source_data.Pass(), cobalt_context_, gr_context_,
      self_message_loop_));
}

scoped_ptr<RawImageMemory> SkiaHardwareResourceProvider::AllocateRawImageMemory(
    size_t size_in_bytes, size_t alignment) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::AllocateRawImageMemory()");
  return scoped_ptr<RawImageMemory>(new SkiaHardwareRawImageMemory(
      cobalt_context_->system_egl()->AllocateRawTextureMemory(size_in_bytes,
                                                              alignment)));
}

scoped_refptr<Image>
SkiaHardwareResourceProvider::CreateMultiPlaneImageFromRawMemory(
    scoped_ptr<RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  TRACE_EVENT0(
      "cobalt::renderer",
      "SkiaHardwareResourceProvider::CreateMultiPlaneImageFromRawMemory()");
  DCHECK_EQ(render_tree::kMultiPlaneImageFormatYUV3PlaneBT709,
            descriptor.image_format())
      << "Currently we only support 3-plane YUV multi plane images.";
  DCHECK_EQ(3, descriptor.num_planes());

  scoped_ptr<SkiaHardwareRawImageMemory> skia_hardware_raw_image_memory(
      base::polymorphic_downcast<SkiaHardwareRawImageMemory*>(
          raw_image_memory.release()));

  return make_scoped_refptr(new SkiaHardwareMultiPlaneImage(
      skia_hardware_raw_image_memory.Pass(), descriptor, cobalt_context_,
      gr_context_, self_message_loop_));
}

bool SkiaHardwareResourceProvider::HasLocalFontFamily(
    const char* font_family_name) const {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::HasLocalFontFamily()");

  SkAutoTUnref<SkFontMgr> fm(SkFontMgr::RefDefault());
  SkAutoTUnref<SkFontStyleSet> style_set(fm->matchFamily(font_family_name));
  return style_set->count() > 0;
}

scoped_refptr<render_tree::Typeface>
SkiaHardwareResourceProvider::GetLocalTypeface(
    const char* font_family_name, render_tree::FontStyle font_style) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::GetLocalTypeface()");

  SkAutoTUnref<SkFontMgr> fm(SkFontMgr::RefDefault());
  SkAutoTUnref<SkTypeface> typeface(fm->matchFamilyStyle(
      font_family_name, CobaltFontStyleToSkFontStyle(font_style)));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SkiaHardwareResourceProvider::GetCharacterFallbackTypeface(
    int32 character, render_tree::FontStyle font_style,
    const std::string& language) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::GetCharacterFallbackTypeface()");

  SkAutoTUnref<SkFontMgr> fm(SkFontMgr::RefDefault());
  SkAutoTUnref<SkTypeface> typeface(
      fm->matchFamilyStyleCharacter(0, CobaltFontStyleToSkFontStyle(font_style),
                                    language.c_str(), character));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
SkiaHardwareResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data,
    std::string* error_string) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareResourceProvider::CreateFontFromData()");

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
SkiaHardwareResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider) {
  return text_shaper_.CreateGlyphBuffer(text_buffer, text_length, language,
                                        is_rtl, font_provider);
}

scoped_refptr<render_tree::GlyphBuffer>
SkiaHardwareResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string,
    const scoped_refptr<render_tree::Font>& font) {
  return text_shaper_.CreateGlyphBuffer(utf8_string, font);
}

float SkiaHardwareResourceProvider::GetTextWidth(
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
