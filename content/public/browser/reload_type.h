// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RELOAD_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_RELOAD_TYPE_H_

namespace content {

// Used to specify detailed behavior on requesting reloads. NONE is used in
// general, but behaviors depend on context. If NONE is used for tab restore, or
// history navigation, it loads preferring cache (which may be stale).
enum class ReloadType {
  // Normal load, restore, or history navigation.
  NONE,
  // Reloads the current entry validating only the main resource.
  NORMAL,
  // Reloads the current entry bypassing the cache (shift-reload).
  BYPASSING_CACHE,
  // Reloads the current entry using the original URL used to create it.  This
  // is used for cases where the user wants to refresh a page using a different
  // user agent after following a redirect. It is also used in the case of an
  // intervention (i.e., preview) being served on the page and the user
  // requesting the page without the intervention.
  ORIGINAL_REQUEST_URL
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RELOAD_TYPE_H_
