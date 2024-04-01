// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/webrtc_features.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace features {
namespace {
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
constexpr base::FeatureState kWebRtcHybridAgcState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kWebRtcHybridAgcState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif
}  // namespace

// When enabled we will tell WebRTC that we want to use the
// Windows.Graphics.Capture API based DesktopCapturer, if it is available.
const base::Feature kWebRtcAllowWgcDesktopCapturer{
    "AllowWgcDesktopCapturer", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables multichannel capture audio to be processed without downmixing in the
// WebRTC audio processing module.
const base::Feature kWebRtcEnableCaptureMultiChannelApm{
    "WebRtcEnableCaptureMultiChannelApm", base::FEATURE_ENABLED_BY_DEFAULT};

// Kill-switch allowing deactivation of the support for 48 kHz internal
// processing in the WebRTC audio processing module when running on an ARM
// platform.
const base::Feature kWebRtcAllow48kHzProcessingOnArm{
    "WebRtcAllow48kHzProcessingOnArm", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the WebRTC Hybrid AGC configuration - i.e., AGC1 analog and AGC2
// digital (see http://crbug.com/1231085).
const base::Feature kWebRtcHybridAgc{"WebRtcHybridAgc", kWebRtcHybridAgcState};

// Enables and configures the clipping control in the WebRTC analog AGC.
const base::Feature kWebRtcAnalogAgcClippingControl{
    "WebRtcAnalogAgcClippingControl", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the override for the default minimum starting volume of the Automatic
// Gain Control algorithm in WebRTC.
const base::Feature kWebRtcAnalogAgcStartupMinVolume{
    "WebRtcAnalogAgcStartupMinVolume", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
