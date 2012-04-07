// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties.h"

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/stringprintf.h"

namespace net {

const char kAlternateProtocolHeader[] = "Alternate-Protocol";
const char* const kAlternateProtocolStrings[] = {
  "npn-spdy/1",
  "npn-spdy/2",
  "npn-spdy/2.1",
  "npn-spdy/3",
};

static const char* AlternateProtocolToString(AlternateProtocol protocol) {
  switch (protocol) {
    case NPN_SPDY_1:
    case NPN_SPDY_2:
    case NPN_SPDY_21:
    case NPN_SPDY_3:
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

HttpServerProperties::HttpServerProperties()
    : has_deletion_stack_trace_(false),
      liveness_token_(TOKEN_ALIVE) {
}

HttpServerProperties::~HttpServerProperties() {
  // Crash if this is a double free!
  CheckIsAlive();

  // Mark the object as dead.
  liveness_token_ = TOKEN_DEAD;

  if (!has_deletion_stack_trace_) {
    // Save the current thread's stack trace.
    has_deletion_stack_trace_ = true;
    deletion_stack_trace_ = base::debug::StackTrace();
  }

  // I doubt this is necessary to prevent optimization, but it can't hurt.
  base::debug::Alias(&liveness_token_);
  base::debug::Alias(&has_deletion_stack_trace_);
  base::debug::Alias(&deletion_stack_trace_);
}

void HttpServerProperties::CheckIsAlive() {
  // Copy the deletion stacktrace onto stack in case we crash.
  base::debug::StackTrace deletion_stack_trace = deletion_stack_trace_;
  base::debug::Alias(&deletion_stack_trace);

  // Copy the token onto stack in case it mismatches so we can explore its
  // value.
  LivenessToken liveness_token = liveness_token_;
  base::debug::Alias(&liveness_token);

  CHECK_EQ(liveness_token, TOKEN_ALIVE);
}

}  // namespace net
