// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_util.h"

#include "build/build_config.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <fontconfig/fontconfig.h>
#include "ui/gfx/linux/fontconfig_util.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/direct_write.h"
#endif

namespace gfx {

void InitializeFonts() {
  // Implicit initialization can cause a long delay on the first rendering if
  // the font cache has to be regenerated for some reason. Doing it explicitly
  // here helps in cases where the browser process is starting up in the
  // background (resources have not yet been granted to cast) since it prevents
  // the long delay the user would have seen on first rendering.

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Ensures the config is created on this thread.
  FcConfig* config = GetGlobalFontConfig();
  DCHECK(config);
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN)
  gfx::win::InitializeDirectWrite();
#endif  // OS_WIN
}

}  // namespace gfx
