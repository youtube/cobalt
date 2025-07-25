// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_KIT_DEVICE_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_KIT_DEVICE_MAC_H_

#include <stdint.h>

#include <memory>

#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"

namespace media {
class VideoCaptureDevice;
}  // namespace media

namespace content {

std::unique_ptr<media::VideoCaptureDevice> CONTENT_EXPORT
CreateScreenCaptureKitDeviceMac(const DesktopMediaID& source);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_SCREEN_CAPTURE_KIT_DEVICE_MAC_H_
