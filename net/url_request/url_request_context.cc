// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

#include "base/string_util.h"

const std::string& URLRequestContext::GetUserAgent(const GURL& url) const {
  return EmptyString();
}
