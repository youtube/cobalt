// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_MEDIA_SWITCHES_H_
#define MEDIA_BASE_MEDIA_SWITCHES_H_

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace switches {

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
extern const char kAlsaOutputDevice[];
extern const char kAlsaInputDevice[];
#endif

#if defined(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
#endif

#if defined(USE_PULSEAUDIO)
MEDIA_EXPORT extern const char kUsePulseAudio[];
#endif

#if defined(OS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
#endif

MEDIA_EXPORT extern const char kDisableAudioFallback[];

MEDIA_EXPORT extern const char kDisableAudioOutputResampler[];

MEDIA_EXPORT extern const char kEnableAudioMixer[];

MEDIA_EXPORT extern const char kEnableWebAudioInput[];

MEDIA_EXPORT extern const char kVideoThreads[];

}  // namespace switches

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
