// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_METRICS_MAC_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_METRICS_MAC_H_

#import <AVFoundation/AVFoundation.h>
#include <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include "third_party/chromium/media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

namespace media_m96 {

CAPTURE_EXPORT
void LogFirstCapturedVideoFrame(const AVCaptureDeviceFormat* bestCaptureFormat,
                                const CMSampleBufferRef buffer);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_METRICS_MAC_H_