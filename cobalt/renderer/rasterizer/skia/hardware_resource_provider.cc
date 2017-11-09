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

#include "cobalt/renderer/rasterizer/skia/hardware_resource_provider.h"

#include <GLES2/gl2ext.h>

#include "base/debug/trace_event.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/font.h"
#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/hardware_image.h"
#include "cobalt/renderer/rasterizer/skia/hardware_mesh.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/typeface.h"
#include "third_party/ots/include/opentype-sanitiser.h"
#include "third_party/ots/include/ots-memory-stream.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

using cobalt::render_tree::Image;
using cobalt::render_tree::ImageData;
using cobalt::render_tree::RawImageMemory;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

HardwareResourceProvider::HardwareResourceProvider(
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    SubmitOffscreenCallback submit_offscreen_callback,
    bool purge_skia_font_caches_on_destruction)
    : cobalt_context_(cobalt_context),
      gr_context_(gr_context),
      submit_offscreen_callback_(submit_offscreen_callback),
      purge_skia_font_caches_on_destruction_(
          purge_skia_font_caches_on_destruction),
      max_texture_size_(gr_context->getMaxTextureSize()),
      self_message_loop_(MessageLoop::current()) {
  // Initialize the font manager now to ensure that it doesn't get initialized
  // on multiple threads simultaneously later.
  SkSafeUnref(SkFontMgr::RefDefault());

#if SB_HAS(GRAPHICS)
  decode_target_graphics_context_provider_.egl_display =
      cobalt_context_->system_egl()->GetDisplay();
  decode_target_graphics_context_provider_.egl_context =
      cobalt_context_->GetContext();
  decode_target_graphics_context_provider_.gles_context_runner =
      &HardwareResourceProvider::GraphicsContextRunner;
  decode_target_graphics_context_provider_.gles_context_runner_context = this;
#endif  // SB_HAS(GRAPHICS)
}

HardwareResourceProvider::~HardwareResourceProvider() {
  if (purge_skia_font_caches_on_destruction_) {
    text_shaper_.PurgeCaches();

    SkAutoTUnref<SkFontMgr> font_manager(SkFontMgr::RefDefault());
    SkFontMgr_Cobalt* cobalt_font_manager =
        base::polymorphic_downcast<SkFontMgr_Cobalt*>(font_manager.get());
    cobalt_font_manager->PurgeCaches();
  }
}

void HardwareResourceProvider::Finish() {
  // Wait for any resource-related to complete (by waiting for all tasks to
  // complete).
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->WaitForFence();
  }
}

bool HardwareResourceProvider::PixelFormatSupported(
    render_tree::PixelFormat pixel_format) {
  return pixel_format == render_tree::kPixelFormatRGBA8 ||
         pixel_format == render_tree::kPixelFormatUYVY;
}

bool HardwareResourceProvider::AlphaFormatSupported(
    render_tree::AlphaFormat alpha_format) {
  return alpha_format == render_tree::kAlphaFormatPremultiplied ||
         alpha_format == render_tree::kAlphaFormatOpaque;
}

scoped_ptr<ImageData> HardwareResourceProvider::AllocateImageData(
    const math::Size& size, render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::AllocateImageData()");
  DCHECK(PixelFormatSupported(pixel_format));
  DCHECK(AlphaFormatSupported(alpha_format));

  if (size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    return scoped_ptr<ImageData>(nullptr);
  }

  return scoped_ptr<ImageData>(new HardwareImageData(
      cobalt_context_->system_egl()->AllocateTextureData(
          size, ConvertRenderTreeFormatToGL(pixel_format)),
      pixel_format, alpha_format));
}

scoped_refptr<render_tree::Image> HardwareResourceProvider::CreateImage(
    scoped_ptr<ImageData> source_data) {
  TRACE_EVENT0("cobalt::renderer", "HardwareResourceProvider::CreateImage()");
  scoped_ptr<HardwareImageData> skia_hardware_source_data(
      base::polymorphic_downcast<HardwareImageData*>(source_data.release()));
  const render_tree::ImageDataDescriptor& descriptor =
      skia_hardware_source_data->GetDescriptor();

  DCHECK(descriptor.alpha_format == render_tree::kAlphaFormatPremultiplied ||
         descriptor.alpha_format == render_tree::kAlphaFormatOpaque);
#if defined(COBALT_BUILD_TYPE_DEBUG)
  Image::DCheckForPremultipliedAlpha(descriptor.size, descriptor.pitch_in_bytes,
                                     descriptor.pixel_format,
                                     skia_hardware_source_data->GetMemory());
#endif

  // Construct a frontend image from this data, which internally will send
  // a message to the rasterizer thread passing along the image data where the
  // backend texture will be constructed, and associated with this frontend
  // texture through a map that will be accessed when the rasterizer visits
  // any subsequently submitted render trees referencing the frontend image.
  return make_scoped_refptr(new HardwareFrontendImage(
      skia_hardware_source_data.Pass(), cobalt_context_, gr_context_,
      self_message_loop_));
}

#if SB_HAS(GRAPHICS)
namespace {

#if SB_API_VERSION < 6
int PlanesPerFormat(SbDecodeTargetFormat format) {
  switch (format) {
    case kSbDecodeTargetFormat1PlaneRGBA:
      return 1;
    case kSbDecodeTargetFormat1PlaneBGRA:
      return 1;
    case kSbDecodeTargetFormat2PlaneYUVNV12:
      return 2;
    case kSbDecodeTargetFormat3PlaneYUVI420:
      return 3;
    default:
      NOTREACHED();
      return 0;
  }
}
#endif  // SB_API_VERSION < 6

uint32_t DecodeTargetFormatToGLFormat(SbDecodeTargetFormat format, int plane,
    const SbDecodeTargetInfoPlane* plane_info) {
  switch (format) {
    case kSbDecodeTargetFormat1PlaneRGBA:
#if SB_API_VERSION >= 6
    // For UYVY, we will use a fragment shader where R = the first U, G = Y,
    // B = the second U, and A = V.
    case kSbDecodeTargetFormat1PlaneUYVY:
#endif  // SB_API_VERSION >= 6
    {
      DCHECK_EQ(0, plane);
      return GL_RGBA;
    } break;
    case kSbDecodeTargetFormat2PlaneYUVNV12: {
      DCHECK_LT(plane, 2);
#if SB_API_VERSION >= 7
      // If this DCHECK fires, please set gl_texture_format, introduced
      // in Starboard 7.
      //
      // You probably want to set it to GL_ALPHA on plane 0 (luma) and
      // GL_LUMINANCE_ALPHA on plane 1 (chroma), which was the default before.
      DCHECK_NE(plane_info->gl_texture_format, 0);
      switch (plane_info->gl_texture_format) {
        case GL_ALPHA:
        case GL_LUMINANCE_ALPHA:
#if defined(GL_RED_EXT)
        case GL_RED_EXT:
#endif
#if defined(GL_RG_EXT)
        case GL_RG_EXT:
#endif
          return plane_info->gl_texture_format;
        default:
          // gl_texture_format is either unassigned or assigned to
          // an invalud value. Please see comment above for
          // gl_texture_format change, introduced in Starboard 7.
          CHECK(false);
          return 0;
      }
#else  // SB_API_VERSION >= 7
      switch (plane) {
        case 0:
          return GL_ALPHA;
        case 1:
          return GL_LUMINANCE_ALPHA;
        default:
          NOTREACHED();
          return GL_RGBA;
      }
#endif  // SB_API_VERSION >= 7
    } break;
    case kSbDecodeTargetFormat3PlaneYUVI420: {
      DCHECK_LT(plane, 3);
      return GL_ALPHA;
    } break;
    default: {
      NOTREACHED();
      return GL_RGBA;
    }
  }
}

render_tree::MultiPlaneImageFormat
DecodeTargetFormatToRenderTreeMultiPlaneFormat(SbDecodeTargetFormat format) {
  switch (format) {
    case kSbDecodeTargetFormat2PlaneYUVNV12: {
      return render_tree::kMultiPlaneImageFormatYUV2PlaneBT709;
    } break;
    case kSbDecodeTargetFormat3PlaneYUVI420: {
      return render_tree::kMultiPlaneImageFormatYUV3PlaneBT709;
    } break;
    default: { NOTREACHED(); }
  }
  return render_tree::kMultiPlaneImageFormatYUV2PlaneBT709;
}

// Helper class that effectively "ref-count-izes" a SbDecodeTarget so that it
// can be shared by multiple consumers and only released when we are done with
// it.
class DecodeTargetReferenceCounted
    : public base::RefCountedThreadSafe<DecodeTargetReferenceCounted> {
 public:
  explicit DecodeTargetReferenceCounted(SbDecodeTarget decode_target)
      : decode_target_(decode_target) {}

 private:
  ~DecodeTargetReferenceCounted() { SbDecodeTargetRelease(decode_target_); }

  SbDecodeTarget decode_target_;

  friend class base::RefCountedThreadSafe<DecodeTargetReferenceCounted>;
};

void DoNothing(scoped_refptr<DecodeTargetReferenceCounted> decode_target_ref) {
  // Dummy function that lets us retain a reference to decode_target_ref within
  // a closure.
}

}  // namespace

scoped_refptr<render_tree::Image>
    HardwareResourceProvider::CreateImageFromSbDecodeTarget(
        SbDecodeTarget decode_target) {
  SbDecodeTargetInfo info;
  SbMemorySet(&info, 0, sizeof(info));
  CHECK(SbDecodeTargetGetInfo(decode_target, &info));
  DCHECK_NE(kSbDecodeTargetFormat1PlaneBGRA, info.format);

  std::vector<scoped_refptr<HardwareFrontendImage> > planes;
  scoped_refptr<DecodeTargetReferenceCounted> decode_target_ref(
      new DecodeTargetReferenceCounted(decode_target));

  // There is limited format support at this time.
#if SB_API_VERSION >= 6
  int planes_per_format = SbDecodeTargetNumberOfPlanesForFormat(info.format);
#else
  int planes_per_format = PlanesPerFormat(info.format);
#endif  // SB_API_VERSION >= 6

  for (int i = 0; i < planes_per_format; ++i) {
    const SbDecodeTargetInfoPlane& plane = info.planes[i];

    int gl_handle = plane.texture;
    render_tree::AlphaFormat alpha_format =
        info.is_opaque ? render_tree::kAlphaFormatOpaque
                       : render_tree::kAlphaFormatUnpremultiplied;

    scoped_ptr<math::Rect> content_region;
    if (plane.content_region.left != 0 || plane.content_region.top != 0 ||
        plane.content_region.right != plane.width ||
        plane.content_region.bottom != plane.height) {
      content_region.reset(new math::Rect(
          plane.content_region.left, plane.content_region.top,
          plane.content_region.right - plane.content_region.left,
          plane.content_region.bottom - plane.content_region.top));
    }

    uint32_t gl_format = DecodeTargetFormatToGLFormat(info.format, i, &plane);

    scoped_ptr<backend::TextureEGL> texture(new backend::TextureEGL(
        cobalt_context_, gl_handle, math::Size(plane.width, plane.height),
        gl_format, plane.gl_texture_target,
        base::Bind(&DoNothing, decode_target_ref)));

    // If the decode target is specified as UYVY format, then we need to pass
    // this in as supplementary data, as the |texture| object only knows that
    // it is RGBA.
    base::optional<AlternateRgbaFormat> alternate_rgba_format;
#if SB_API_VERSION >= 6
    if (info.format == kSbDecodeTargetFormat1PlaneUYVY) {
      alternate_rgba_format = AlternateRgbaFormat_UYVY;
    }
#endif  // SB_API_VERSION >= 6

    planes.push_back(make_scoped_refptr(new HardwareFrontendImage(
        texture.Pass(), alpha_format, cobalt_context_, gr_context_,
        content_region.Pass(), self_message_loop_, alternate_rgba_format)));
  }

  if (planes_per_format == 1) {
    return planes[0];
  } else {
    return new HardwareMultiPlaneImage(
        DecodeTargetFormatToRenderTreeMultiPlaneFormat(info.format), planes);
  }
}

namespace {
void RunGraphicsContextRunnerOnRasterizerThread(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context,
    backend::GraphicsContextEGL* graphics_context,
    base::WaitableEvent* done_event) {
  backend::GraphicsContextEGL::ScopedMakeCurrent make_current(graphics_context);
  target_function(target_function_context);
  done_event->Signal();
}
}  // namespace

// static
void HardwareResourceProvider::GraphicsContextRunner(
    SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  SbDecodeTarget decode_target = kSbDecodeTargetInvalid;

  HardwareResourceProvider* provider =
      reinterpret_cast<HardwareResourceProvider*>(
          graphics_context_provider->gles_context_runner_context);

  if (MessageLoop::current() != provider->self_message_loop_) {
    // Post a task to the rasterizer thread to have it run the requested
    // function, and wait for it to complete before returning.
    base::WaitableEvent done_event(true, false);
    provider->self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&RunGraphicsContextRunnerOnRasterizerThread,
                              target_function, target_function_context,
                              provider->cobalt_context_, &done_event));
    done_event.Wait();
  } else {
    // If we are already on the rasterizer thread, just run the function
    // directly.
    backend::GraphicsContextEGL::ScopedMakeCurrent make_current(
        provider->cobalt_context_);
    target_function(target_function_context);
  }
}

#endif  // SB_HAS(GRAPHICS)

scoped_ptr<RawImageMemory> HardwareResourceProvider::AllocateRawImageMemory(
    size_t size_in_bytes, size_t alignment) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::AllocateRawImageMemory()");
  return scoped_ptr<RawImageMemory>(new HardwareRawImageMemory(
      cobalt_context_->system_egl()->AllocateRawTextureMemory(size_in_bytes,
                                                              alignment)));
}

scoped_refptr<render_tree::Image>
HardwareResourceProvider::CreateMultiPlaneImageFromRawMemory(
    scoped_ptr<RawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor) {
  TRACE_EVENT0(
      "cobalt::renderer",
      "HardwareResourceProvider::CreateMultiPlaneImageFromRawMemory()");
  DCHECK((render_tree::kMultiPlaneImageFormatYUV2PlaneBT709 ==
              descriptor.image_format() &&
          2 == descriptor.num_planes()) ||
         (render_tree::kMultiPlaneImageFormatYUV3PlaneBT709 ==
              descriptor.image_format() &&
          3 == descriptor.num_planes()))
      << "Currently we only support 2-plane or 3-plane YUV multi plane images.";

  scoped_ptr<HardwareRawImageMemory> skia_hardware_raw_image_memory(
      base::polymorphic_downcast<HardwareRawImageMemory*>(
          raw_image_memory.release()));

  return make_scoped_refptr(new HardwareMultiPlaneImage(
      skia_hardware_raw_image_memory.Pass(), descriptor, cobalt_context_,
      gr_context_, self_message_loop_));
}

bool HardwareResourceProvider::HasLocalFontFamily(
    const char* font_family_name) const {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::HasLocalFontFamily()");

  SkAutoTUnref<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  SkAutoTUnref<SkFontStyleSet> style_set(
      font_manager->matchFamily(font_family_name));
  return style_set->count() > 0;
}

scoped_refptr<render_tree::Typeface> HardwareResourceProvider::GetLocalTypeface(
    const char* font_family_name, render_tree::FontStyle font_style) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::GetLocalTypeface()");

  SkAutoTUnref<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  SkAutoTUnref<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          font_manager->matchFamilyStyle(
              font_family_name, CobaltFontStyleToSkFontStyle(font_style))));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
HardwareResourceProvider::GetLocalTypefaceByFaceNameIfAvailable(
    const char* font_face_name) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::GetLocalTypefaceIfAvailable()");

  SkAutoTUnref<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  SkFontMgr_Cobalt* cobalt_font_manager =
      base::polymorphic_downcast<SkFontMgr_Cobalt*>(font_manager.get());

  SkTypeface_Cobalt* typeface = base::polymorphic_downcast<SkTypeface_Cobalt*>(
      cobalt_font_manager->MatchFaceName(font_face_name));
  if (typeface != NULL) {
    SkAutoTUnref<SkTypeface> typeface_unref_helper(typeface);
    return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
  }

  return NULL;
}

scoped_refptr<render_tree::Typeface>
HardwareResourceProvider::GetCharacterFallbackTypeface(
    int32 character, render_tree::FontStyle font_style,
    const std::string& language) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::GetCharacterFallbackTypeface()");

  SkAutoTUnref<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  SkAutoTUnref<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          font_manager->matchFamilyStyleCharacter(
              NULL, CobaltFontStyleToSkFontStyle(font_style), language.c_str(),
              character)));
  return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
}

scoped_refptr<render_tree::Typeface>
HardwareResourceProvider::CreateTypefaceFromRawData(
    scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data,
    std::string* error_string) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareResourceProvider::CreateFontFromData()");

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

  SkAutoTUnref<SkStreamAsset> stream(new SkMemoryStream(skia_data));
  SkAutoTUnref<SkTypeface_Cobalt> typeface(
      base::polymorphic_downcast<SkTypeface_Cobalt*>(
          SkTypeface::CreateFromStream(stream)));
  if (typeface) {
    return scoped_refptr<render_tree::Typeface>(new SkiaTypeface(typeface));
  } else {
    *error_string = "Skia unable to create typeface";
    return NULL;
  }
}

scoped_refptr<render_tree::GlyphBuffer>
HardwareResourceProvider::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider) {
  return text_shaper_.CreateGlyphBuffer(text_buffer, text_length, language,
                                        is_rtl, font_provider);
}

scoped_refptr<render_tree::GlyphBuffer>
HardwareResourceProvider::CreateGlyphBuffer(
    const std::string& utf8_string,
    const scoped_refptr<render_tree::Font>& font) {
  return text_shaper_.CreateGlyphBuffer(utf8_string, font);
}

float HardwareResourceProvider::GetTextWidth(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider,
    render_tree::FontVector* maybe_used_fonts) {
  return text_shaper_.GetTextWidth(text_buffer, text_length, language, is_rtl,
                                   font_provider, maybe_used_fonts);
}

scoped_refptr<render_tree::Mesh> HardwareResourceProvider::CreateMesh(
    scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
    render_tree::Mesh::DrawMode draw_mode) {
  return new HardwareMesh(vertices.Pass(), draw_mode);
}

scoped_refptr<render_tree::Image> HardwareResourceProvider::DrawOffscreenImage(
    const scoped_refptr<render_tree::Node>& root) {
  return make_scoped_refptr(new HardwareFrontendImage(
      root, submit_offscreen_callback_, cobalt_context_, gr_context_,
      self_message_loop_));
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
