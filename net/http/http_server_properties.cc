// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  "npn-spdy/3",
};
const char kBrokenAlternateProtocol[] = "Broken";

const char* AlternateProtocolToString(AlternateProtocol protocol) {
  switch (protocol) {
    case NPN_SPDY_1:
    case NPN_SPDY_2:
    case NPN_SPDY_3:
      return kAlternateProtocolStrings[protocol];
    case ALTERNATE_PROTOCOL_BROKEN:
      return kBrokenAlternateProtocol;
    case UNINITIALIZED_ALTERNATE_PROTOCOL:
      return "Uninitialized";
    default:
      NOTREACHED();
      return "";
  }
}

AlternateProtocol AlternateProtocolFromString(const std::string& protocol) {
  for (int i = NPN_SPDY_1; i < NUM_ALTERNATE_PROTOCOLS; ++i)
    if (protocol == kAlternateProtocolStrings[i])
      return static_cast<AlternateProtocol>(i);
  if (protocol == kBrokenAlternateProtocol)
    return ALTERNATE_PROTOCOL_BROKEN;
  return UNINITIALIZED_ALTERNATE_PROTOCOL;
}


std::string PortAlternateProtocolPair::ToString() const {
  return base::StringPrintf("%d:%s", port,
                            AlternateProtocolToString(protocol));
}

}  // namespace net
