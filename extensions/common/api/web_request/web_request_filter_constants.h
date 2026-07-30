// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_CONSTANTS_H_
#define EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_CONSTANTS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "extensions/common/url_pattern.h"

namespace extensions {

// The URL schemes a `webRequest.RequestFilter` URL pattern can match.
inline constexpr int kWebRequestFilterValidSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_EXTENSION | URLPattern::SCHEME_WS |
    URLPattern::SCHEME_WSS | URLPattern::SCHEME_UUID_IN_PACKAGE;

// Request/response header names only delivered to webRequest listeners that
// registered the "extraHeaders" option. Lowercase for case-insensitive
// comparison.
inline constexpr auto kExtraRequestHeaderNames =
    base::MakeFixedFlatSet<std::string_view>(
        {"accept-encoding", "accept-language", "cookie", "origin", "referer"});
inline constexpr auto kExtraResponseHeaderNames =
    base::MakeFixedFlatSet<std::string_view>({"set-cookie"});

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_CONSTANTS_H_
