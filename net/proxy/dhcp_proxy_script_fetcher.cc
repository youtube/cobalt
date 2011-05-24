// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/dhcp_proxy_script_fetcher.h"

#include "net/base/net_errors.h"

namespace net {

DhcpProxyScriptFetcher::DhcpProxyScriptFetcher() {
}

DhcpProxyScriptFetcher::~DhcpProxyScriptFetcher() {
}

std::string DhcpProxyScriptFetcher::GetFetcherName() const {
  return "";
}

DoNothingDhcpProxyScriptFetcher::DoNothingDhcpProxyScriptFetcher() {
}

DoNothingDhcpProxyScriptFetcher::~DoNothingDhcpProxyScriptFetcher() {
}

int DoNothingDhcpProxyScriptFetcher::Fetch(string16* utf16_text,
                                           CompletionCallback* callback) {
  return ERR_NOT_IMPLEMENTED;
}

void DoNothingDhcpProxyScriptFetcher::Cancel() {
}

const GURL& DoNothingDhcpProxyScriptFetcher::GetPacURL() const {
  return gurl_;
}

}  // namespace net
