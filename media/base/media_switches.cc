// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

namespace switches {

#if defined(OS_LINUX)
// The Alsa device to use when opening an audio stream.
const char kAlsaDevice[] = "alsa-device";
#endif

const char kVideoH264Annexb[] = "video-h264-annexb";

}  // namespace switches
