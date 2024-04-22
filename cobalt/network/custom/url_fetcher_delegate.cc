// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/network/custom/url_fetcher_delegate.h"

namespace net {

#if defined(STARBOARD)
void URLFetcherDelegate::OnURLFetchResponseStarted(const URLFetcher* source) {}
#endif  // defined(STARBOARD)

void URLFetcherDelegate::OnURLFetchDownloadProgress(
    const URLFetcher* source, int64_t current, int64_t total,
    int64_t current_network_bytes) {}

void URLFetcherDelegate::OnURLFetchUploadProgress(const URLFetcher* source,
                                                  int64_t current,
                                                  int64_t total) {}

URLFetcherDelegate::~URLFetcherDelegate() = default;

}  // namespace net
