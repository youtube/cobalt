// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_SWITCHES_H_
#define MEDIA_BASE_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

#if defined(OS_LINUX)
extern const char kAlsaDevice[];
#endif

}  // namespace switches

#endif  // MEDIA_BASE_SWITCHES_H_
