// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CHROMEOS_PUBLIC_CROS_FEATURES_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CHROMEOS_PUBLIC_CROS_FEATURES_H_

namespace media_m96 {

// A run-time check for whether or not we should use the OS-level camera
// service on ChromeOS for video capture.
bool ShouldUseCrosCameraService();

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CHROMEOS_PUBLIC_CROS_FEATURES_H_
