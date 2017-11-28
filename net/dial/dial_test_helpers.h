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
  MockServiceHandler(const std::string& service_name)
      : service_name_(service_name) {}
  MOCK_METHOD3(HandleRequest,
               void(const std::string&,
                    const HttpServerRequestInfo& request,
                    const CompletionCB& completion_cb));
  const std::string& service_name() const override { return service_name_; }

 private:
  ~MockServiceHandler() {}
  std::string service_name_;
};

} // namespace net

#endif // NET_DIAL_DIAL_TEST_HELPERS_H

