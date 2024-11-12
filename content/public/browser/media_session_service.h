// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_SERVICE_H_

#include "content/common/content_export.h"
#include "services/media_session/public/cpp/media_session_service.h"

namespace content {

// Returns the main control interface into the Media Session Service which runs
// in the browser process.
CONTENT_EXPORT media_session::MediaSessionService& GetMediaSessionService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_SERVICE_H_
