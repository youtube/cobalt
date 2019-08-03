// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_delegate.h"

namespace net {

void URLFetcherDelegate::OnURLFetchDownloadProgress(
    const URLFetcher* source, int64 current, int64 total) {}

void URLFetcherDelegate::OnURLFetchDownloadData(
    const URLFetcher* source, scoped_ptr<std::string> download_data) {}

void URLFetcherDelegate::OnURLFetchUploadProgress(
    const URLFetcher* source, int64 current, int64 total) {}

bool URLFetcherDelegate::ShouldSendDownloadData() {
  return false;
}

URLFetcherDelegate::~URLFetcherDelegate() {}

}  // namespace net
