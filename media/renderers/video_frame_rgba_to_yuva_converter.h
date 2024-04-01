// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_RGBA_TO_YUVA_CONVERTER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_RGBA_TO_YUVA_CONVERTER_H_

#include "components/viz/common/resources/resource_format.h"
#include "media/base/media_export.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace gpu {
struct MailboxHolder;
struct SyncToken;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;

// Copy the specified source texture to the destination video frame, doing
// color space conversion and RGB to YUV conversion. Waits for all sync
// tokens in `src_mailbox_holder` and `dst_video_frame` before doing the
// copy. Populates `completion_sync_token` with a sync token after the copy.
// Updates `dst_video_frame`'s sync token.
MEDIA_EXPORT bool CopyRGBATextureToVideoFrame(
    viz::RasterContextProvider* raster_context_provider,
    viz::ResourceFormat src_format,
    const gfx::Size& src_size,
    const gfx::ColorSpace& src_color_space,
    GrSurfaceOrigin src_surface_origin,
    const gpu::MailboxHolder& src_mailbox_holder,
    media::VideoFrame* dst_video_frame,
    gpu::SyncToken& completion_sync_token);

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_RGBA_TO_YUVA_CONVERTER_H_
