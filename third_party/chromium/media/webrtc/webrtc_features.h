// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines features for media/webrtc.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_WEBRTC_WEBRTC_FEATURES_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_WEBRTC_WEBRTC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::Feature kWebRtcAllowWgcDesktopCapturer;

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::Feature kWebRtcEnableCaptureMultiChannelApm;

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::Feature kWebRtcAllow48kHzProcessingOnArm;

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::Feature kWebRtcHybridAgc;

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::Feature kWebRtcAnalogAgcClippingControl;

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::Feature kWebRtcAnalogAgcStartupMinVolume;

}  // namespace features

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_WEBRTC_WEBRTC_FEATURES_H_
