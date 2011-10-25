// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service_defaults.h"

namespace net {

SSLConfigServiceDefaults::SSLConfigServiceDefaults() {
}

void SSLConfigServiceDefaults::GetSSLConfig(SSLConfig* config) {
  *config = default_config_;
  SetSSLConfigFlags(config);
  config->crl_set = GetCRLSet();
}

SSLConfigServiceDefaults::~SSLConfigServiceDefaults() {
}

}  // namespace net
