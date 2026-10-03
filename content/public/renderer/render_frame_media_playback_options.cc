// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame_media_playback_options.h"

#include "build/android_buildflags.h"
#include "build/build_config.h"

namespace content {

// Cobalt destroys the native SbWindow on conceal and must suspend media
// pipelines when the page is hidden so SbPlayer instances are released.
#if (BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)) || \
    BUILDFLAG(IS_COBALT)
const bool kIsBackgroundMediaSuspendEnabled = true;
#else
const bool kIsBackgroundMediaSuspendEnabled = false;
#endif

}  // namespace content
