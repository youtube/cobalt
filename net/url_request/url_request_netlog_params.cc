// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_netlog_params.h"

#include "base/values.h"
#include "googleurl/src/gurl.h"

namespace net {

Value* NetLogURLRequestStartCallback(const GURL* url,
                                     const std::string* method,
                                     int load_flags,
                                     RequestPriority priority,
                                     NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("url", url->possibly_invalid_spec());
  dict->SetString("method", *method);
  dict->SetInteger("load_flags", load_flags);
  dict->SetInteger("priority", static_cast<int>(priority));
  return dict;
}

bool StartEventLoadFlagsFromEventParams(const Value* event_params,
                                        int* load_flags) {
  const DictionaryValue* dict;
  if (!event_params->GetAsDictionary(&dict) ||
      !dict->GetInteger("load_flags", load_flags)) {
    *load_flags = 0;
    return false;
  }
  return true;
}

}  // namespace net
