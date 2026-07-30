// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_UTILS_H_

#include "content/common/content_export.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

// Returns true if the PrefetchActivationBeacon feature is enabled.
//
// Since this feature is checked in the browser process, and the origin trial
// token is checked against response headers, we manually validate the origin
// trial token from the headers here if the base::Feature is disabled by
// default. If the feature is explicitly kill-switched via Finch/command-line
// (overridden to disabled), this will return false.
CONTENT_EXPORT bool IsPrefetchActivationBeaconEnabled(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers);

// Returns true if the PrerenderActivationBeacon feature is enabled.
//
// Similar to IsPrefetchActivationBeaconEnabled, we manually validate the origin
// trial token from the headers here if the base::Feature is disabled by
// default, and respect any explicit override (kill-switch).
CONTENT_EXPORT bool IsPrerenderActivationBeaconEnabled(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_UTILS_H_
