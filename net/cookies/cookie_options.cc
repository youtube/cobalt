// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#include "net/cookies/cookie_options.h"

namespace net {

// Keep default values in sync with content/public/common/cookie_manager.mojom.
CookieOptions::CookieOptions()
    : exclude_httponly_(true),
      same_site_cookie_mode_(SameSiteCookieMode::DO_NOT_INCLUDE),
#if defined(STARBOARD)
      update_access_time_(false),
#else
      update_access_time_(true),
#endif
      server_time_() {}

}  // namespace net
