// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
#define EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_

#include "content/public/browser/notification_types.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/411569.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace extensions {

// Only notifications fired by the extensions module should be here. The
// extensions module should not listen to notifications fired by the
// embedder.
enum NotificationType {
  // WARNING: This need to match chrome/browser/chrome_notification_types.h.
  NOTIFICATION_EXTENSIONS_START = content::NOTIFICATION_CONTENT_END,

  // An error occurred during extension install. The details are a string with
  // details about why the install failed.
  // TODO(https://crbug.com/1174734): Remove.
  NOTIFICATION_EXTENSION_INSTALL_ERROR,

  NOTIFICATION_EXTENSIONS_END
};

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/411569.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
