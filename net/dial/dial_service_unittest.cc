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

  virtual void TearDown() OVERRIDE {
    service_.reset();
  }

  void GetHandlerTest(const std::string& path,
                      const std::string& exp_path,
                      const DialServiceHandler* exp_handler) {
    std::string ret_path;
    DialServiceHandler* ret_handler = service_->GetHandler(path, &ret_path);
    EXPECT_EQ(ret_path, exp_path);
    EXPECT_EQ(ret_handler, exp_handler);
  }
};

TEST_F(DialServiceTest, GetHandler) {
  service_.reset(new DialService());

  MockServiceHandler foo_handler("Foo");
  MockServiceHandler baz_handler("Baz");

  service_->Register(&foo_handler);
  service_->Register(&baz_handler);

  GetHandlerTest("/apps/Foo/run", "/run", &foo_handler);
  GetHandlerTest("/apps/Baz/404", "/404", &baz_handler);
  GetHandlerTest("/apps/Baz/404/etc", "/404/etc", &baz_handler);
  GetHandlerTest("/apps/Foo/", "/", &foo_handler);
  GetHandlerTest("/apps/Foo", "/", &foo_handler);

  GetHandlerTest("", "", NULL);
  GetHandlerTest("/", "", NULL);
  GetHandlerTest("/apps1/Foo/run", "", NULL);
  GetHandlerTest("/apps/Bar/foo", "", NULL);
  GetHandlerTest("/apps/FooBar/", "", NULL);
  GetHandlerTest("/apps/BarFoo/", "", NULL);

  service_->Deregister(&foo_handler);
  service_->Deregister(&baz_handler);
  service_->Terminate();
}

// Test the case where Deregister is called and immediately after the handler
// is destroyed. This makes a call to the dial_service thread with a pointer
// that is destroyed. Make sure that it handles appropriately.
TEST_F(DialServiceTest, DestructedHandler) {
  service_.reset(new DialService());

  MockServiceHandler* foo_handler = new MockServiceHandler("Foo");
  service_->Register(foo_handler);

  // Immediately delete foo_handler after calling Deregister.
  service_->Deregister(foo_handler);
  delete foo_handler;

  service_->Terminate();
}

} // namespace net

