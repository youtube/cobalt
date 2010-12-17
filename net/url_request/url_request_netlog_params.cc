// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_netlog_params.h"

#include "base/values.h"

namespace net {

URLRequestStartEventParameters::URLRequestStartEventParameters(
    const GURL& url,
    const std::string& method,
    int load_flags,
    RequestPriority priority)
    : url_(url),
      method_(method),
      load_flags_(load_flags),
      priority_(priority) {
}

Value* URLRequestStartEventParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("url", url_.possibly_invalid_spec());
  dict->SetString("method", method_);
  dict->SetInteger("load_flags", load_flags_);
  dict->SetInteger("priority", static_cast<int>(priority_));
  return dict;
}

}  // namespace net
