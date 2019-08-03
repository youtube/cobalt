// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_net_log_parameters.h"

#include "base/values.h"

namespace net {

base::Value* NetLogFileStreamErrorCallback(
    FileErrorSource source,
    int os_error,
    net::Error net_error,
    NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("operation", GetFileErrorSourceName(source));
  dict->SetInteger("os_error", os_error);
  dict->SetInteger("net_error", net_error);

  return dict;
}

}  // namespace net
