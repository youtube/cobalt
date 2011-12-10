// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace net {

const char kAlternateProtocolHeader[] = "Alternate-Protocol";
const char* const kAlternateProtocolStrings[] = {
  "npn-spdy/1",
  "npn-spdy/2",
  "npn-spdy/2.1",
};

static const char* AlternateProtocolToString(AlternateProtocol protocol) {
  switch (protocol) {
    case NPN_SPDY_1:
    case NPN_SPDY_2:
    case NPN_SPDY_21:
      return kAlternateProtocolStrings[protocol];
    case ALTERNATE_PROTOCOL_BROKEN:
      return "Broken";
    case UNINITIALIZED_ALTERNATE_PROTOCOL:
      return "Uninitialized";
    default:
      NOTREACHED();
      return "";
  }
}

std::string PortAlternateProtocolPair::ToString() const {
  return base::StringPrintf("%d:%s", port,
                            AlternateProtocolToString(protocol));
}

}  // namespace net
