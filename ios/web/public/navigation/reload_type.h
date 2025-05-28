// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_RELOAD_TYPE_H_
#define IOS_WEB_PUBLIC_NAVIGATION_RELOAD_TYPE_H_

namespace web {

// Used to specify detailed behavior on requesting reload.
enum class ReloadType : short {
  // Reloads the visible item.
  NORMAL = 0,

  // Reloads the pending item, or the last committed item if no pending item
  // exists, using the original URL used to create it. This is used for cases
  // where the user wants to refresh a page using a different configuration
  // (e.g. user agent).
  ORIGINAL_REQUEST_URL,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_RELOAD_TYPE_H_
