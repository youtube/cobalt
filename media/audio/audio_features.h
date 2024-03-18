// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_FEATURES_H_
#define MEDIA_AUDIO_AUDIO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace features {

MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioServiceOutOfProcessKillAtHang);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDumpOnAudioServiceHang);

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAAudioDriver);
#endif

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowIAudioClient3);
#endif

}  // namespace features

#endif  // MEDIA_AUDIO_AUDIO_FEATURES_H_
