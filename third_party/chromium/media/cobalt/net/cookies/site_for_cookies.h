// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SITE_FOR_COOKIES_H
#define NET_SITE_FOR_COOKIES_H

#if !defined(STARBOARD)
#error "This file only works with Cobalt/Starboard."
#endif  // !defined(STARBOARD)

namespace net {

// Reduced version enough to make media code depending on it to be built.
class SiteForCookies {
};

}  // namespace net

#endif  // NET_SITE_FOR_COOKIES_H
