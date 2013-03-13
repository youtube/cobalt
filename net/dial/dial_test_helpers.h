// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DIAL_DIAL_TEST_HELPERS_H
#define NET_DIAL_DIAL_TEST_HELPERS_H

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "net/dial/dial_service.h"
#include "net/dial/dial_service_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MockServiceHandler : public DialServiceHandler {
 public:
  MOCK_METHOD4(handleRequest, bool(const std::string&,
                                   const HttpServerRequestInfo&,
                                   HttpServerResponseInfo*,
                                   const base::Callback<void(bool)>&));
  MOCK_CONST_METHOD0(service_name, const std::string());
};

// Blocks the running thread until |message_loop| runs all previously posted
// messages. Note: This has no guarantee on a DelayedTask.
static void WaitUntilIdle(MessageLoop* message_loop) {
  // Because if you do this, you are gonna get stuck forever!
  ASSERT_NE(MessageLoop::current(), message_loop);
  CHECK(message_loop);

  scoped_ptr<base::WaitableEvent> waiter;
  waiter.reset(new base::WaitableEvent(true /* manual_reset */,
                                       false /* initially_signalled */));
  message_loop->PostTask(FROM_HERE, base::Bind(
      &base::WaitableEvent::Signal, base::Unretained(waiter.get())));
  ASSERT_TRUE(waiter->TimedWait(base::TimeDelta::FromMilliseconds(100)));
}

} // namespace net

#endif // NET_DIAL_DIAL_TEST_HELPERS_H

