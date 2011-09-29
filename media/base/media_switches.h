// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

MEDIA_EXPORT extern const char kUsePulseAudio[];
MEDIA_EXPORT extern const char kVideoThreads[];

}  // namespace switches

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
