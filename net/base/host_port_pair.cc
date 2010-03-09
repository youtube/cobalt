// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"
#include "base/string_util.h"

namespace net {

std::string HostPortPair::ToString() const {
  return StringPrintf("[Host: %s, Port: %u]", host.c_str(), port);
}

}  // namespace net
