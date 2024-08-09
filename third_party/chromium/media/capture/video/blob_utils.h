// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_BLOB_UTILS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_BLOB_UTILS_H_

#include "third_party/chromium/media/capture/mojom/image_capture.mojom.h"

namespace media_m96 {

struct VideoCaptureFormat;

// Helper method to create a mojom::Blob out of |buffer|, whose pixel format and
// resolution are taken from |capture_format|. |rotation| is the clockwise
// rotation to be applied to the frame. The value can only be 0, 90, 180 or 270.
// It's only effective if |capture_format| is PIXEL_FORMAT_MJPEG.
// Returns a null BlobPtr in case of error.
mojom::BlobPtr RotateAndBlobify(const uint8_t* buffer,
                                const uint32_t bytesused,
                                const VideoCaptureFormat& capture_format,
                                const int rotation);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_BLOB_UTILS_H_
