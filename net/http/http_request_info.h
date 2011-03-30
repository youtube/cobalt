// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_REQUEST_INFO_H__
#define NET_HTTP_HTTP_REQUEST_INFO_H__
#pragma once

#include <string>
#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/base/request_priority.h"
#include "net/base/upload_data.h"
#include "net/http/http_request_headers.h"

namespace net {

struct HttpRequestInfo {
  enum RequestMotivation{
    // TODO(mbelshe): move these into Client Socket.
    PRECONNECT_MOTIVATED,  // Request was motivated by a prefetch.
    OMNIBOX_MOTIVATED,     // Request was motivated by the omnibox.
    NORMAL_MOTIVATION,     // No special motivation associated with the request.
    EARLY_LOAD_MOTIVATED,  // When browser asks a tab to open an URL, this short
                           // circuits that path (of waiting for the renderer to
                           // do the URL request), and starts loading ASAP.
  };

  HttpRequestInfo();
  ~HttpRequestInfo();

  // The requested URL.
  GURL url;

  // The referring URL (if any).
  GURL referrer;

  // The method to use (GET, POST, etc.).
  std::string method;

  // Any extra request headers (including User-Agent).
  HttpRequestHeaders extra_headers;

  // Any upload data.
  scoped_refptr<UploadData> upload_data;

  // Any load flags (see load_flags.h).
  int load_flags;

  // The priority level for this request.
  RequestPriority priority;

  // The motivation behind this request.
  RequestMotivation motivation;

  // An optional globally unique identifier for this request for use by the
  // consumer. 0 is invalid.
  uint64 request_id;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_INFO_H__
