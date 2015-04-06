// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DIAL_DIAL_TEST_HELPERS_H
#define NET_DIAL_DIAL_TEST_HELPERS_H

#include "base/message_loop.h"
#include "net/dial/dial_service_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MockServiceHandler : public DialServiceHandler {
 public:
  MockServiceHandler(const std::string& service_name);
  MOCK_METHOD4(handleRequest, bool(const std::string&,
                                   const HttpServerRequestInfo&,
                                   HttpServerResponseInfo*,
                                   const base::Callback<void(bool)>&));
  const std::string service_name() const OVERRIDE {
    return service_name_;
  }
 private:
  std::string service_name_;
};

// Blocks the running thread until |message_loop| runs all previously posted
// messages. Note: This has no guarantee on a DelayedTask.
void WaitUntilIdle(base::MessageLoopProxy* message_loop);

} // namespace net

#endif // NET_DIAL_DIAL_TEST_HELPERS_H

