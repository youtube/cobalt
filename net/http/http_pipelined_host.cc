// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host.h"

namespace net {

HttpPipelinedHost::Key::Key(const HostPortPair& origin)
    : origin_(origin) {
}

bool HttpPipelinedHost::Key::operator<(const Key& rhs) const {
  return origin_ < rhs.origin_;
}

}  // namespace net
