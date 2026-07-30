// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist.h"

#include <algorithm>

#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "url/gurl.h"

namespace network {

ConnectionAllowlist::ConnectionAllowlist() = default;
ConnectionAllowlist::~ConnectionAllowlist() = default;

ConnectionAllowlist::ConnectionAllowlist(ConnectionAllowlist&&) = default;
ConnectionAllowlist& ConnectionAllowlist::operator=(ConnectionAllowlist&&) =
    default;

ConnectionAllowlist::ConnectionAllowlist(const ConnectionAllowlist&) = default;
ConnectionAllowlist& ConnectionAllowlist::operator=(
    const ConnectionAllowlist&) = default;

bool ConnectionAllowlist::operator==(const ConnectionAllowlist& other) const =
    default;

bool ConnectionAllowlistMatchesUrl(
    const ConnectionAllowlist& connection_allowlist,
    const GURL& url) {
  // Local schemes don't touch the network, so they bypass the allowlist.
  if (url.SchemeIsLocal()) {
    return true;
  }
  for (const auto& url_string : connection_allowlist.allowlist) {
    auto matcher = url_pattern::SimpleUrlPatternMatcher::Create(
        url_string, /*base_url=*/nullptr);
    if (!matcher.has_value()) {
      // TODO(crbug.com/447954811): This case should result in an issue
      // delivered to the devtools console (and ideally we'd avoid it
      // entirely by parsing these strings as URL Patterns when initially
      // parsing the header rather than here when enforcing it).
      continue;
    }
    if (matcher.value()->Match(url)) {
      return true;
    }
  }

  return false;
}

bool ConnectionAllowlistSubsumes(const ConnectionAllowlist& required,
                                 const ConnectionAllowlist& candidate) {
  // Every endpoint the candidate would permit must also be permitted by the
  // required allowlist. An empty candidate allowlist permits no network
  // endpoints and is therefore trivially subsumed (it is maximally strict).
  for (const auto& entry : candidate.allowlist) {
    if (!std::ranges::contains(required.allowlist, entry)) {
      return false;
    }
  }

  // `candidate` must also be at least as strict as `required` for redirects and
  // WebRTC. kBlock is stricter than kAllow, so the only disallowed combination
  // is `required` blocking while `candidate` would allow.
  if (required.redirect_behavior ==
          ConnectionAllowlist::RedirectBehavior::kBlock &&
      candidate.redirect_behavior ==
          ConnectionAllowlist::RedirectBehavior::kAllow) {
    return false;
  }
  if (required.webrtc_behavior == ConnectionAllowlist::WebRtcBehavior::kBlock &&
      candidate.webrtc_behavior ==
          ConnectionAllowlist::WebRtcBehavior::kAllow) {
    return false;
  }

  return true;
}

ConnectionAllowlists::ConnectionAllowlists() = default;
ConnectionAllowlists::~ConnectionAllowlists() = default;

ConnectionAllowlists::ConnectionAllowlists(ConnectionAllowlists&&) = default;
ConnectionAllowlists& ConnectionAllowlists::operator=(ConnectionAllowlists&&) =
    default;

ConnectionAllowlists::ConnectionAllowlists(const ConnectionAllowlists&) =
    default;
ConnectionAllowlists& ConnectionAllowlists::operator=(
    const ConnectionAllowlists&) = default;

bool ConnectionAllowlists::operator==(const ConnectionAllowlists& other) const =
    default;

}  // namespace network
