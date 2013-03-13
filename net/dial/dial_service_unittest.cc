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
  scoped_ptr<DialService> service;

  virtual void TearDown() OVERRIDE {
    service.reset();
  }

  void WaitUntilIdle() {
    net::WaitUntilIdle(service->GetMessageLoop());
  }

  void PostGetHandlerTest(const std::string& path,
                          const std::string&  exp_path,
                          const DialServiceHandler* exp_handler) {
    service->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
        &DialServiceTest::DoGetHandlerTest, base::Unretained(this),
        path, exp_path, exp_handler));
    WaitUntilIdle();
  }

  void DoGetHandlerTest(const std::string& path,
                        const std::string& exp_path,
                        const DialServiceHandler* exp_handler) {
    EXPECT_EQ(MessageLoop::current(), service->GetMessageLoop());

    std::string ret_path;
    DialServiceHandler* ret_handler = service->GetHandler(path, &ret_path);
    EXPECT_EQ(ret_path, exp_path);
    EXPECT_EQ(ret_handler, exp_handler);
  }
};

TEST_F(DialServiceTest, GetHandler) {
  service.reset(new DialService());

  MockServiceHandler foo_handler;
  MockServiceHandler baz_handler;
  ON_CALL(foo_handler, service_name()).WillByDefault(Return("Foo"));
  ON_CALL(baz_handler, service_name()).WillByDefault(Return("Baz"));

  EXPECT_FALSE(service->is_running());

  service->Register(&foo_handler);
  WaitUntilIdle();
  EXPECT_TRUE(service->is_running());

  service->Register(&baz_handler);
  WaitUntilIdle();
  EXPECT_TRUE(service->is_running());

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

  service->Deregister(&foo_handler);
  WaitUntilIdle();
  EXPECT_TRUE(service->is_running());

  service->Deregister(&baz_handler);
  WaitUntilIdle();
  EXPECT_FALSE(service->is_running());
}

} // namespace net

