// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_error_params.h"

#include "base/values.h"

namespace net {

SSLErrorParams::SSLErrorParams(int net_error, int ssl_lib_error)
    : net_error_(net_error), ssl_lib_error_(ssl_lib_error) {}

SSLErrorParams::~SSLErrorParams() {}

Value* SSLErrorParams::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("net_error", net_error_);
  if (ssl_lib_error_)
    dict->SetInteger("ssl_lib_error", ssl_lib_error_);
  return dict;
}

}  // namespace net
