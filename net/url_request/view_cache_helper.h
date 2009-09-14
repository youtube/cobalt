// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
#define NET_URL_REQUEST_VIEW_CACHE_HELPER_H_

#include <string>

class URLRequestContext;

class ViewCacheHelper {
 public:
  // Formats the cache information for |key| as HTML.
  static void GetEntryInfoHTML(const std::string& key,
                               URLRequestContext* context,
                               const std::string& url_prefix,
                               std::string* out);

  static void GetStatisticsHTML(URLRequestContext* context,
                                std::string* out);
};

#endif  // NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
