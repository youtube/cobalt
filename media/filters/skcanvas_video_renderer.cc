// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/skcanvas_video_renderer.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace media {

static bool IsEitherYV12OrYV16(media::VideoFrame::Format format) {
  return format == media::VideoFrame::YV12 || format == media::VideoFrame::YV16;
}

static bool IsEitherYV12OrYV16OrNative(media::VideoFrame::Format format) {
  return IsEitherYV12OrYV16(format) ||
      format == media::VideoFrame::NATIVE_TEXTURE;
}

// CanFastPaint is a helper method to determine the conditions for fast
// painting. The conditions are:
// 1. No skew in canvas matrix.
// 2. No flipping nor mirroring.
// 3. Canvas has pixel format ARGB8888.
// 4. Canvas is opaque.
// 5. Frame format is YV12 or YV16.
//
// TODO(hclam): The fast paint method should support flipping and mirroring.
// Disable the flipping and mirroring checks once we have it.
static bool CanFastPaint(SkCanvas* canvas, uint8_t alpha,
                         media::VideoFrame::Format format) {
  if (alpha != 0xFF || !IsEitherYV12OrYV16(format))
    return false;

  const SkMatrix& total_matrix = canvas->getTotalMatrix();
  // Perform the following checks here:
  // 1. Check for skewing factors of the transformation matrix. They should be
  //    zero.
  // 2. Check for mirroring and flipping. Make sure they are greater than zero.
  if (SkScalarNearlyZero(total_matrix.getSkewX()) &&
      SkScalarNearlyZero(total_matrix.getSkewY()) &&
      total_matrix.getScaleX() > 0 &&
      total_matrix.getScaleY() > 0) {
    SkDevice* device = canvas->getDevice();
    const SkBitmap::Config config = device->config();

    if (config == SkBitmap::kARGB_8888_Config && device->isOpaque()) {
      return true;
    }
  }

  return false;
}

// Fast paint does YUV => RGB, scaling, blitting all in one step into the
// canvas. It's not always safe and appropriate to perform fast paint.
// CanFastPaint() is used to determine the conditions.
static void FastPaint(
    const scoped_refptr<media::VideoFrame>& video_frame,
    SkCanvas* canvas,
    const SkRect& dest_rect) {
  DCHECK(IsEitherYV12OrYV16(video_frame->format())) << video_frame->format();
  DCHECK_EQ(video_frame->stride(media::VideoFrame::kUPlane),
            video_frame->stride(media::VideoFrame::kVPlane));

  const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(true);
  media::YUVType yuv_type = media::YV16;
  int y_shift = 0;
  if (video_frame->format() == media::VideoFrame::YV12) {
    yuv_type = media::YV12;
    y_shift = 1;
  }

  // Transform the destination rectangle to local coordinates.
  const SkMatrix& local_matrix = canvas->getTotalMatrix();
  SkRect local_dest_rect;
  local_matrix.mapRect(&local_dest_rect, dest_rect);

  // After projecting the destination rectangle to local coordinates, round
  // the projected rectangle to integer values, this will give us pixel values
  // of the rectangle.
  SkIRect local_dest_irect, local_dest_irect_saved;
  local_dest_rect.round(&local_dest_irect);
  local_dest_rect.round(&local_dest_irect_saved);

  // No point painting if the destination rect doesn't intersect with the
  // clip rect.
  if (!local_dest_irect.intersect(canvas->getTotalClip().getBounds()))
    return;

  // At this point |local_dest_irect| contains the rect that we should draw
  // to within the clipping rect.

  // Calculate the address for the top left corner of destination rect in
  // the canvas that we will draw to. The address is obtained by the base
  // address of the canvas shifted by "left" and "top" of the rect.
  uint8* dest_rect_pointer = static_cast<uint8*>(bitmap.getPixels()) +
      local_dest_irect.fTop * bitmap.rowBytes() +
      local_dest_irect.fLeft * 4;

  // Project the clip rect to the original video frame, obtains the
  // dimensions of the projected clip rect, "left" and "top" of the rect.
  // The math here are all integer math so we won't have rounding error and
  // write outside of the canvas.
  // We have the assumptions of dest_rect.width() and dest_rect.height()
  // being non-zero, these are valid assumptions since finding intersection
  // above rejects empty rectangle so we just do a DCHECK here.
  DCHECK_NE(0, dest_rect.width());
  DCHECK_NE(0, dest_rect.height());
  size_t frame_clip_width = local_dest_irect.width() *
      video_frame->visible_rect().width() / local_dest_irect_saved.width();
  size_t frame_clip_height = local_dest_irect.height() *
      video_frame->visible_rect().height() / local_dest_irect_saved.height();

  // Project the "left" and "top" of the final destination rect to local
  // coordinates of the video frame, use these values to find the offsets
  // in the video frame to start reading.
  size_t frame_clip_left =
      video_frame->visible_rect().x() +
      (local_dest_irect.fLeft - local_dest_irect_saved.fLeft) *
      video_frame->visible_rect().width() / local_dest_irect_saved.width();
  size_t frame_clip_top =
      video_frame->visible_rect().y() +
      (local_dest_irect.fTop - local_dest_irect_saved.fTop) *
      video_frame->visible_rect().height() / local_dest_irect_saved.height();

  // Use the "left" and "top" of the destination rect to locate the offset
  // in Y, U and V planes.
  size_t y_offset = (video_frame->stride(media::VideoFrame::kYPlane) *
                     frame_clip_top) + frame_clip_left;

  // For format YV12, there is one U, V value per 2x2 block.
  // For format YV16, there is one U, V value per 2x1 block.
  size_t uv_offset = (video_frame->stride(media::VideoFrame::kUPlane) *
                      (frame_clip_top >> y_shift)) + (frame_clip_left >> 1);
  uint8* frame_clip_y =
      video_frame->data(media::VideoFrame::kYPlane) + y_offset;
  uint8* frame_clip_u =
      video_frame->data(media::VideoFrame::kUPlane) + uv_offset;
  uint8* frame_clip_v =
      video_frame->data(media::VideoFrame::kVPlane) + uv_offset;

  // TODO(hclam): do rotation and mirroring here.
  // TODO(fbarchard): switch filtering based on performance.
  bitmap.lockPixels();
  media::ScaleYUVToRGB32(frame_clip_y,
                         frame_clip_u,
                         frame_clip_v,
                         dest_rect_pointer,
                         frame_clip_width,
                         frame_clip_height,
                         local_dest_irect.width(),
                         local_dest_irect.height(),
                         video_frame->stride(media::VideoFrame::kYPlane),
                         video_frame->stride(media::VideoFrame::kUPlane),
                         bitmap.rowBytes(),
                         yuv_type,
                         media::ROTATE_0,
                         media::FILTER_BILINEAR);
  bitmap.unlockPixels();
}

// Converts a VideoFrame containing YUV data to a SkBitmap containing RGB data.
//
// |bitmap| will be (re)allocated to match the dimensions of |video_frame|.
static void ConvertVideoFrameToBitmap(
    const scoped_refptr<media::VideoFrame>& video_frame,
    SkBitmap* bitmap) {
  DCHECK(IsEitherYV12OrYV16OrNative(video_frame->format()))
      << video_frame->format();
  if (IsEitherYV12OrYV16(video_frame->format())) {
    DCHECK_EQ(video_frame->stride(media::VideoFrame::kUPlane),
              video_frame->stride(media::VideoFrame::kVPlane));
  }

  // Check if |bitmap| needs to be (re)allocated.
  if (bitmap->isNull() ||
      bitmap->width() != video_frame->visible_rect().width() ||
      bitmap->height() != video_frame->visible_rect().height()) {
    bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                      video_frame->visible_rect().width(),
                      video_frame->visible_rect().height());
    bitmap->allocPixels();
    bitmap->setIsVolatile(true);
  }

  bitmap->lockPixels();
  if (IsEitherYV12OrYV16(video_frame->format())) {
    media::YUVType yuv_type = media::YV16;
    int y_shift = 0;
    if (video_frame->format() == media::VideoFrame::YV12) {
      yuv_type = media::YV12;
      y_shift = 1;
    }

    // Use the "left" and "top" of the destination rect to locate the offset
    // in Y, U and V planes.
    size_t y_offset = (video_frame->stride(media::VideoFrame::kYPlane) *
                       video_frame->visible_rect().y()) +
                      video_frame->visible_rect().x();

    // For format YV12, there is one U, V value per 2x2 block.
    // For format YV16, there is one U, V value per 2x1 block.
    size_t uv_offset = (video_frame->stride(media::VideoFrame::kUPlane) *
                        (video_frame->visible_rect().y() >> y_shift)) +
                       (video_frame->visible_rect().x() >> 1);
    uint8* frame_clip_y =
        video_frame->data(media::VideoFrame::kYPlane) + y_offset;
    uint8* frame_clip_u =
        video_frame->data(media::VideoFrame::kUPlane) + uv_offset;
    uint8* frame_clip_v =
        video_frame->data(media::VideoFrame::kVPlane) + uv_offset;

    media::ConvertYUVToRGB32(frame_clip_y,
                             frame_clip_u,
                             frame_clip_v,
                             static_cast<uint8*>(bitmap->getPixels()),
                             video_frame->visible_rect().width(),
                             video_frame->visible_rect().height(),
                             video_frame->stride(media::VideoFrame::kYPlane),
                             video_frame->stride(media::VideoFrame::kUPlane),
                             bitmap->rowBytes(),
                             yuv_type);
  } else {
    DCHECK_EQ(video_frame->format(), media::VideoFrame::NATIVE_TEXTURE);
    video_frame->ReadPixelsFromNativeTexture(bitmap->getPixels());
  }
  bitmap->notifyPixelsChanged();
  bitmap->unlockPixels();
}

SkCanvasVideoRenderer::SkCanvasVideoRenderer()
    : last_frame_timestamp_(media::kNoTimestamp()) {
}

SkCanvasVideoRenderer::~SkCanvasVideoRenderer() {}

void SkCanvasVideoRenderer::Paint(media::VideoFrame* video_frame,
                                  SkCanvas* canvas,
                                  const gfx::RectF& dest_rect,
                                  uint8_t alpha) {
  if (alpha == 0) {
    return;
  }

  SkRect dest;
  dest.set(dest_rect.x(), dest_rect.y(), dest_rect.right(), dest_rect.bottom());

  SkPaint paint;
  paint.setAlpha(alpha);

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame || !IsEitherYV12OrYV16OrNative(video_frame->format())) {
    canvas->drawRect(dest, paint);
    return;
  }

  // Scale and convert to RGB in one step if we can.
  if (CanFastPaint(canvas, alpha, video_frame->format())) {
    FastPaint(video_frame, canvas, dest);
    return;
  }

  // Check if we should convert and update |last_frame_|.
  if (last_frame_.isNull() ||
      video_frame->GetTimestamp() != last_frame_timestamp_) {
    ConvertVideoFrameToBitmap(video_frame, &last_frame_);
    last_frame_timestamp_ = video_frame->GetTimestamp();
  }

  // Do a slower paint using |last_frame_|.
  paint.setFilterBitmap(true);
  canvas->drawBitmapRect(last_frame_, NULL, dest, &paint);
}

}  // namespace media
