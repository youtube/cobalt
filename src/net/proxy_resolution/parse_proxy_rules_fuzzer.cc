// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config.h"
#include "starboard/types.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::ProxyConfig::ProxyRules rules;
  std::string input(data, data + size);
  rules.ParseFromString(input);
  return 0;
}
