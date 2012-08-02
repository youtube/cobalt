// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_UTIL_H_
#define MEDIA_BASE_VIDEO_UTIL_H_

#include "base/basictypes.h"
#include "media/base/media_export.h"
#include "ui/gfx/size.h"

namespace media {

class VideoFrame;

// Computes the size of |visible_size| for a given aspect ratio.
MEDIA_EXPORT gfx::Size GetNaturalSize(const gfx::Size& visible_size,
                                      int aspect_ratio_numerator,
                                      int aspect_ratio_denominator);

// Copies a plane of YUV source into a VideoFrame object, taking into account
// source and destinations dimensions.
//
// NOTE: rows is *not* the same as height!
MEDIA_EXPORT void CopyYPlane(const uint8* source, int stride, int rows,
                             VideoFrame* frame);
MEDIA_EXPORT void CopyUPlane(const uint8* source, int stride, int rows,
                             VideoFrame* frame);
MEDIA_EXPORT void CopyVPlane(const uint8* source, int stride, int rows,
                             VideoFrame* frame);

// Fills |frame| containing YUV data to the given color values.
MEDIA_EXPORT void FillYUV(VideoFrame* frame, uint8 y, uint8 u, uint8 v);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_UTIL_H_
