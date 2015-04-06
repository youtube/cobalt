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

  void WaitUntilIdle() {
    net::WaitUntilIdle(service_->message_loop_proxy());
  }

  void PostGetHandlerTest(const std::string& path,
                          const std::string&  exp_path,
                          const DialServiceHandler* exp_handler) {
    service_->message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
        &DialServiceTest::DoGetHandlerTest, base::Unretained(this),
        path, exp_path, exp_handler));
    WaitUntilIdle();
  }

  void DoGetHandlerTest(const std::string& path,
                        const std::string& exp_path,
                        const DialServiceHandler* exp_handler) {
    EXPECT_TRUE(service_->IsOnServiceThread());

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
  EXPECT_FALSE(service_->is_running());

  service_->Register(&foo_handler);
  WaitUntilIdle();
  EXPECT_TRUE(service_->is_running());

  service_->Register(&baz_handler);
  WaitUntilIdle();
  EXPECT_TRUE(service_->is_running());

  PostGetHandlerTest("/apps/Foo/run", "/run", &foo_handler);
  PostGetHandlerTest("/apps/Baz/404", "/404", &baz_handler);
  PostGetHandlerTest("/apps/Baz/404/etc", "/404/etc", &baz_handler);
  PostGetHandlerTest("/apps/Foo/", "/", &foo_handler);
  PostGetHandlerTest("/apps/Foo", "/", &foo_handler);

  PostGetHandlerTest("", "", NULL);
  PostGetHandlerTest("/", "", NULL);
  PostGetHandlerTest("/apps1/Foo/run", "", NULL);
  PostGetHandlerTest("/apps/Bar/foo", "", NULL);
  PostGetHandlerTest("/apps/FooBar/", "", NULL);
  PostGetHandlerTest("/apps/BarFoo/", "", NULL);

  service_->Deregister(&foo_handler);
  WaitUntilIdle();
  EXPECT_TRUE(service_->is_running());

  service_->Deregister(&baz_handler);
  WaitUntilIdle();
  service_->Terminate();
  EXPECT_FALSE(service_->is_running());
}

// Test the case where Deregister is called and immediately after the handler
// is destroyed. This makes a call to the dial_service thread with a pointer
// that is destroyed. Make sure that it handles appropriately.
TEST_F(DialServiceTest, DestructedHandler) {
  service_.reset(new DialService());

  MockServiceHandler* foo_handler = new MockServiceHandler("Foo");
  EXPECT_TRUE(service_->Register(foo_handler));
  WaitUntilIdle();
  EXPECT_TRUE(service_->is_running());

  // Put a wait on the dial service thread.
  service_->message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&base::PlatformThread::Sleep,
                 base::TimeDelta::FromMilliseconds(10)));

  // Immediately delete foo_handler after calling Deregister.
  EXPECT_TRUE(service_->Deregister(foo_handler));
  delete foo_handler;

  // Make sure that the actual Deregister method has not kicked in.
  EXPECT_TRUE(service_->is_running());

  // Let all tasks complete on the dial service thread.
  WaitUntilIdle();

  service_->Terminate();
  EXPECT_FALSE(service_->is_running());
}

} // namespace net

