// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_converter.h"

#include <GLES3/gl3.h>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_frame_yuv_mailboxes_holder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace media {

namespace {

SkColorType GetCompatibleSurfaceColorType(GrGLenum format) {
  switch (format) {
    case GL_RGBA8:
      return kRGBA_8888_SkColorType;
    case GL_RGB565:
      return kRGB_565_SkColorType;
    case GL_RGBA16F:
      return kRGBA_F16_SkColorType;
    case GL_RGB8:
      return kRGB_888x_SkColorType;
    case GL_RGB10_A2:
      return kRGBA_1010102_SkColorType;
    case GL_RGBA4:
      return kARGB_4444_SkColorType;
    case GL_SRGB8_ALPHA8:
      return kRGBA_8888_SkColorType;
    default:
      NOTREACHED();
      return kUnknown_SkColorType;
  }
}

GrGLenum GetSurfaceColorFormat(GrGLenum format, GrGLenum type) {
  if (format == GL_RGBA) {
    if (type == GL_UNSIGNED_BYTE)
      return GL_RGBA8;
    if (type == GL_UNSIGNED_SHORT_4_4_4_4)
      return GL_RGBA4;
  }
  if (format == GL_RGB) {
    if (type == GL_UNSIGNED_BYTE)
      return GL_RGB8;
    if (type == GL_UNSIGNED_SHORT_5_6_5)
      return GL_RGB565;
  }
  return format;
}

bool DrawYUVImageToSkSurface(const VideoFrame* video_frame,
                             sk_sp<SkImage> image,
                             sk_sp<SkSurface> surface,
                             bool use_visible_rect) {
  if (!use_visible_rect) {
    surface->getCanvas()->drawImage(image, 0, 0);
  } else {
    // Draw the planar SkImage to the SkSurface wrapping the WebGL texture.
    // Using drawImageRect to draw visible rect from video frame to dst texture.
    const gfx::Rect& visible_rect = video_frame->visible_rect();
    const SkRect src_rect =
        SkRect::MakeXYWH(visible_rect.x(), visible_rect.y(),
                         visible_rect.width(), visible_rect.height());
    const SkRect dst_rect =
        SkRect::MakeWH(visible_rect.width(), visible_rect.height());
    surface->getCanvas()->drawImageRect(image, src_rect, dst_rect,
                                        SkSamplingOptions(), nullptr,
                                        SkCanvas::kStrict_SrcRectConstraint);
  }

  surface->flushAndSubmit();
  return true;
}

bool YUVPixmapsToSkSurface(GrDirectContext* gr_context,
                           const VideoFrame* video_frame,
                           const SkYUVAPixmaps yuva_pixmaps,
                           sk_sp<SkSurface> surface,
                           bool use_visible_rect) {
  auto image =
      SkImage::MakeFromYUVAPixmaps(gr_context, yuva_pixmaps, GrMipmapped::kNo,
                                   false, SkColorSpace::MakeSRGB());

  if (!image) {
    return false;
  }

  return DrawYUVImageToSkSurface(video_frame, image, surface, use_visible_rect);
}

}  // namespace

VideoFrameYUVConverter::VideoFrameYUVConverter() = default;
VideoFrameYUVConverter::~VideoFrameYUVConverter() = default;

bool VideoFrameYUVConverter::IsVideoFrameFormatSupported(
    const VideoFrame& video_frame) {
  return std::get<0>(VideoFrameYUVMailboxesHolder::VideoPixelFormatToSkiaValues(
             video_frame.format())) != SkYUVAInfo::PlaneConfig::kUnknown;
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder) {
  VideoFrameYUVConverter converter;
  return converter.ConvertYUVVideoFrame(video_frame, raster_context_provider,
                                        dest_mailbox_holder);
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrame(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect,
    bool use_sk_pixmap) {
  DCHECK(video_frame);
  DCHECK(IsVideoFrameFormatSupported(*video_frame))
      << "VideoFrame has an unsupported YUV format " << video_frame->format();
  DCHECK(!video_frame->coded_size().IsEmpty())
      << "|video_frame| must have an area > 0";
  DCHECK(raster_context_provider);

  if (!holder_)
    holder_ = std::make_unique<VideoFrameYUVMailboxesHolder>();

  if (raster_context_provider->GrContext()) {
    // Only SW VideoFrame direct uploading path use SkPixmap.
    return ConvertFromVideoFrameYUVWithGrContext(
        video_frame, raster_context_provider, dest_mailbox_holder,
        internal_format, type, flip_y, use_visible_rect, use_sk_pixmap);
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());

  gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes]{};
  holder_->VideoFrameToMailboxes(video_frame, raster_context_provider,
                                 mailboxes);
  ri->ConvertYUVAMailboxesToRGB(dest_mailbox_holder.mailbox,
                                holder_->yuva_info().yuvColorSpace(),
                                holder_->yuva_info().planeConfig(),
                                holder_->yuva_info().subsampling(), mailboxes);
  return true;
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrameToDstTextureNoCaching(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect,
    bool use_sk_pixmap) {
  VideoFrameYUVConverter converter;
  return converter.ConvertYUVVideoFrame(
      video_frame, raster_context_provider, dest_mailbox_holder,
      internal_format, type, flip_y, use_visible_rect, use_sk_pixmap);
}

void VideoFrameYUVConverter::ReleaseCachedData() {
  holder_.reset();
}

bool VideoFrameYUVConverter::ConvertFromVideoFrameYUVWithGrContext(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect,
    bool use_sk_pixmap) {
  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());
  GLuint dest_tex_id =
      ri->CreateAndConsumeForGpuRaster(dest_mailbox_holder.mailbox);
  if (dest_mailbox_holder.mailbox.IsSharedImage()) {
    ri->BeginSharedImageAccessDirectCHROMIUM(
        dest_tex_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  }

  bool result = ConvertFromVideoFrameYUVSkia(
      video_frame, raster_context_provider, dest_mailbox_holder.texture_target,
      dest_tex_id, internal_format, type, flip_y, use_visible_rect,
      use_sk_pixmap);

  if (dest_mailbox_holder.mailbox.IsSharedImage())
    ri->EndSharedImageAccessDirectCHROMIUM(dest_tex_id);
  ri->DeleteGpuRasterTexture(dest_tex_id);

  return result;
}

bool VideoFrameYUVConverter::ConvertFromVideoFrameYUVSkia(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_target,
    unsigned int texture_id,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect,
    bool use_sk_pixmap) {
  // Rendering YUV textures to SkSurface by dst texture
  GrDirectContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);
  // TODO(crbug.com/674185): We should compare the DCHECK vs when
  // UpdateLastImage calls this function.
  DCHECK(IsVideoFrameFormatSupported(*video_frame));

  // Create SkSurface with dst texture.
  GrGLTextureInfo result_gl_texture_info{};
  result_gl_texture_info.fID = texture_id;
  result_gl_texture_info.fTarget = texture_target;
  result_gl_texture_info.fFormat = GetSurfaceColorFormat(internal_format, type);

  int result_width = use_visible_rect ? video_frame->visible_rect().width()
                                      : video_frame->coded_size().width();
  int result_height = use_visible_rect ? video_frame->visible_rect().height()
                                       : video_frame->coded_size().height();

  GrBackendTexture result_texture(result_width, result_height, GrMipMapped::kNo,
                                  result_gl_texture_info);

  // Use dst texture as SkSurface back resource.
  auto surface = SkSurface::MakeFromBackendTexture(
      gr_context, result_texture,
      flip_y ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin, 1,
      GetCompatibleSurfaceColorType(result_gl_texture_info.fFormat),
      SkColorSpace::MakeSRGB(), nullptr);

  // Terminate if surface cannot be created.
  if (!surface) {
    return false;
  }

  bool result = false;
  if (use_sk_pixmap) {
    SkYUVAPixmaps yuva_pixmaps = holder_->VideoFrameToSkiaPixmaps(video_frame);
    DCHECK(yuva_pixmaps.isValid());

    result = YUVPixmapsToSkSurface(gr_context, video_frame, yuva_pixmaps,
                                   surface, use_visible_rect);
  } else {
    auto image =
        holder_->VideoFrameToSkImage(video_frame, raster_context_provider);
    result =
        DrawYUVImageToSkSurface(video_frame, image, surface, use_visible_rect);

    // Release textures to guarantee |holder_| doesn't hold read access on
    // textures it doesn't own.
    holder_->ReleaseTextures();
  }

  return result;
}

}  // namespace media
