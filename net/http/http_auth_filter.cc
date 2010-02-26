// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_filter.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"

namespace net {

HttpAuthFilterWhitelist::HttpAuthFilterWhitelist() {
}

HttpAuthFilterWhitelist::~HttpAuthFilterWhitelist() {
}

bool HttpAuthFilterWhitelist::SetFilters(const std::string& server_whitelist) {
  // Parse the input string using commas as separators.
  rules_.ParseFromString(server_whitelist);
  return true;
}

bool HttpAuthFilterWhitelist::IsValid(const GURL& url,
                                      HttpAuth::Target target) const {
  if ((target != HttpAuth::AUTH_SERVER) && (target != HttpAuth::AUTH_PROXY))
    return false;
  // All proxies pass
  if (target == HttpAuth::AUTH_PROXY)
    return true;
  return rules_.Matches(url);
}

// Add a new domain |filter| to the whitelist, if it's not already there
bool HttpAuthFilterWhitelist::AddFilter(const std::string& filter,
                                        HttpAuth::Target target) {
  if ((target != HttpAuth::AUTH_SERVER) && (target != HttpAuth::AUTH_PROXY))
    return false;
  // All proxies pass
  if (target == HttpAuth::AUTH_PROXY)
    return true;
  rules_.AddRuleFromString(filter);
  return true;
}

void HttpAuthFilterWhitelist::AddRuleToBypassLocal() {
  rules_.AddRuleToBypassLocal();
}

}   // namespace net
