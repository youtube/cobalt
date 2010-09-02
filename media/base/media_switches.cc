// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

namespace switches {

#if defined(OS_LINUX)
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
#endif

// Enable hardware decoding through gpu process.
const char kEnableAcceleratedDecoding[]  = "enable-accelerated-decoding";

// Enable hardware decoding using OpenMax API.
// In practice this is for ChromeOS ARM.
const char kEnableOpenMax[] = "enable-openmax";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

}  // namespace switches
