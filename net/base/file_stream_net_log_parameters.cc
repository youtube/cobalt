// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_net_log_parameters.h"

#include "base/values.h"

namespace net {

FileStreamErrorParameters::FileStreamErrorParameters(
    const std::string& operation, int os_error, net::Error net_error)
        : operation_(operation), os_error_(os_error), net_error_(net_error) {
}

FileStreamErrorParameters::~FileStreamErrorParameters() {}

Value* FileStreamErrorParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("operation", operation_);
  dict->SetInteger("os_error", os_error_);
  dict->SetInteger("net_error", net_error_);

  return dict;
}

}  // namespace net
