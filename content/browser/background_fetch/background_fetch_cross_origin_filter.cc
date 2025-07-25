// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"

#include <set>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kAccessControlAllowOriginHeader[] = "access-control-allow-origin";
const char kAccessControlAllowCredentialsHeader[] =
    "access-control-allow-credentials";
const char kAnyOriginValue[] = "*";

// Parses the header list (including "any origin") value from a given response
// header value, writing the results in |*any_origin| or |*origins|. Returns
// whether all values included in the the |value| is valid.
bool ParseOriginListHeader(const std::string& value,
                           bool* any_origin,
                           std::set<url::Origin>* origins) {
  DCHECK(any_origin);
  DCHECK(origins);

  if (value == kAnyOriginValue) {
    *any_origin = true;
    return true;
  }

  std::set<url::Origin> candidate_origins;
  std::vector<std::string> origin_vector = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (const std::string& origin_string : origin_vector) {
    url::Origin origin = url::Origin::Create(GURL(origin_string));
    if (origin.opaque())
      return false;

    candidate_origins.insert(origin);
  }

  origins->swap(candidate_origins);
  return !origins->empty();
}

}  // namespace

BackgroundFetchCrossOriginFilter::BackgroundFetchCrossOriginFilter(
    const url::Origin& source_origin,
    const BackgroundFetchRequestInfo& request) {
  DCHECK(!request.GetURLChain().empty());

  const GURL& final_url = request.GetURLChain().back();
  const auto& response_header_map = request.GetResponseHeaders();

  // True iff |source_origin| is the same origin as the original request URL.
  is_same_origin_ = source_origin.IsSameOriginWith(final_url);

  // Access-Control-Allow-Origin checks. The header's values must be valid for
  // it to not be completely discarded.
  auto access_control_allow_origin_iter =
      response_header_map.find(kAccessControlAllowOriginHeader);

  if (access_control_allow_origin_iter != response_header_map.end()) {
    std::set<url::Origin> access_control_allow_origins;

    if (ParseOriginListHeader(access_control_allow_origin_iter->second,
                              &access_control_allow_origin_any_,
                              &access_control_allow_origins)) {
      access_control_allow_origin_exact_ =
          access_control_allow_origins.count(source_origin) == 1;
    }
  }

  // Access-Control-Allow-Credentials checks. The header's values must be valid
  // for it to not be completely discarded.
  auto access_control_allow_credentials_iter =
      response_header_map.find(kAccessControlAllowCredentialsHeader);
  if (access_control_allow_credentials_iter != response_header_map.end()) {
    access_control_allow_credentials_ =
        base::ToLowerASCII(access_control_allow_credentials_iter->second) ==
        "true";
  }

  include_credentials_ = request.fetch_request()->credentials_mode ==
                         ::network::mojom::CredentialsMode::kInclude;

  // TODO(crbug.com/711354): Consider the Access-Control-Allow-Headers header.
  // TODO(crbug.com/711354): Consider the Access-Control-Allow-Methods header.
  // TODO(crbug.com/711354): Consider the Access-Control-Expose-Headers header.
}

BackgroundFetchCrossOriginFilter::~BackgroundFetchCrossOriginFilter() = default;

bool BackgroundFetchCrossOriginFilter::CanPopulateBody() const {
  if (is_same_origin_) {
    // Same origin requests are always OK.
    return true;
  }

  // For cross-origin requests, the body will be populated if:

  // (1) The Access-Control-Allow-Origin method allows the source origin / any
  //     origin.
  if (!access_control_allow_origin_exact_ &&
      !access_control_allow_origin_any_) {
    return false;
  }

  // (2) For requests with credentials, the Access-Control-Allow-Credentials is
  //     set and the Access-Control-Allow-Origin contains the exact origin.
  if (include_credentials_ && (!access_control_allow_credentials_ ||
                               !access_control_allow_origin_exact_)) {
    return false;
  }

  return true;
}

}  // namespace content
