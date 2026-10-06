// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_SERVICE_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/media_service.mojom-forward.h"

#if defined(STARBOARD) || defined(ENABLE_BUILDFLAG_IS_COBALT)
#include "base/functional/callback_forward.h"
#endif  // defined(STARBOARD) || defined(ENABLE_BUILDFLAG_IS_COBALT)

namespace content {

// Returns the browser's remote interface to the default global Media Service
// instance, which is started lazily and may run in- or out-of-process.
CONTENT_EXPORT media::mojom::MediaService& GetMediaService();

#if defined(STARBOARD) || defined(ENABLE_BUILDFLAG_IS_COBALT)
// Flushes and suspends active StarboardRenderers in the MediaService if it is
// currently bound, or records the concealed state and runs |done_cb|
// immediately if no MediaService is bound yet. Must be called on the UI thread.
CONTENT_EXPORT void FlushAndSuspendMediaServiceOnUI(base::OnceClosure done_cb);

// Clears the concealed state in the MediaService so subsequent playback or
// pipeline resume requests can create new SbPlayer instances. Must be called on
// the UI thread.
CONTENT_EXPORT void ResumeMediaServiceOnUI();

// Overrides the UI-thread MediaService instance for testing (e.g., in unit
// tests without a GPU process). Passing nullptr resets the override.
CONTENT_EXPORT void OverrideMediaServiceForTesting(
    media::mojom::MediaService* service);
#endif  // defined(STARBOARD) || defined(ENABLE_BUILDFLAG_IS_COBALT)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_SERVICE_H_
