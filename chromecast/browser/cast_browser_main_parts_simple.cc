// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastBrowserMainParts> CastBrowserMainParts::Create(
    CastContentBrowserClient* cast_content_browser_client) {
  return std::make_unique<CastBrowserMainParts>(cast_content_browser_client);
}

}  // namespace shell
}  // namespace chromecast
