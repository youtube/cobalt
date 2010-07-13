// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_netlog_params.h"

#include "base/values.h"

URLRequestStartEventParameters::URLRequestStartEventParameters(
    const GURL& url,
    const std::string& method,
    int load_flags,
    net::RequestPriority priority)
    : url_(url),
      method_(method),
      load_flags_(load_flags),
      priority_(priority) {
}

Value* URLRequestStartEventParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(L"url", url_.possibly_invalid_spec());
  dict->SetString(L"method", method_);
  dict->SetInteger(L"load_flags", load_flags_);
  dict->SetInteger(L"priority", static_cast<int>(priority_));
  return dict;
}
