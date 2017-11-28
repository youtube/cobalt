// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dial_service.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "net/dial/dial_test_helpers.h"
#include "net/server/http_server_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace net {

class DialServiceTest : public testing::Test {
 protected:
  scoped_ptr<DialService> service_;

  virtual void TearDown() override {
    service_.reset();
  }

  void GetHandlerTest(
      const std::string& path,
      const std::string& expected_path,
      const scoped_refptr<DialServiceHandler>& expected_handler) {
    std::string ret_path;
    scoped_refptr<DialServiceHandler> ret_handler =
        service_->GetHandler(path, &ret_path);
    EXPECT_EQ(ret_path, expected_path);
    EXPECT_EQ(ret_handler, expected_handler);
  }
};

TEST_F(DialServiceTest, GetHandler) {
  service_.reset(new DialService());

  scoped_refptr<MockServiceHandler> foo_handler = new MockServiceHandler("Foo");
  scoped_refptr<MockServiceHandler> baz_handler = new MockServiceHandler("Baz");

  service_->Register(foo_handler);
  service_->Register(baz_handler);

  GetHandlerTest("/apps/Foo/run", "/run", foo_handler);
  GetHandlerTest("/apps/Baz/404", "/404", baz_handler);
  GetHandlerTest("/apps/Baz/404/etc", "/404/etc", baz_handler);
  GetHandlerTest("/apps/Foo/", "/", foo_handler);
  GetHandlerTest("/apps/Foo", "/", foo_handler);

  GetHandlerTest("", "", NULL);
  GetHandlerTest("/", "", NULL);
  GetHandlerTest("/apps1/Foo/run", "", NULL);
  GetHandlerTest("/apps/Bar/foo", "", NULL);
  GetHandlerTest("/apps/FooBar/", "", NULL);
  GetHandlerTest("/apps/BarFoo/", "", NULL);

  service_->Deregister(foo_handler);
  service_->Deregister(baz_handler);
  service_->Terminate();
}

// Test the case where Deregister is called and immediately after the handler
// ref is released.
TEST_F(DialServiceTest, ReleasedHandler) {
  service_.reset(new DialService());

  scoped_refptr<MockServiceHandler> foo_handler = new MockServiceHandler("Foo");
  service_->Register(foo_handler);
  service_->Deregister(foo_handler);
  foo_handler = NULL;

  service_->Terminate();
}

} // namespace net

