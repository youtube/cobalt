// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_info.h"

namespace net {

HttpRequestInfo::HttpRequestInfo()
    : load_flags(0),
      priority(LOWEST),
      motivation(NORMAL_MOTIVATION) {
}

HttpRequestInfo::~HttpRequestInfo() {}

}  // namespace net
