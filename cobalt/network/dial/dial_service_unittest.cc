// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/network/dial/dial_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/task_environment.h"
#include "cobalt/network/dial/dial_test_helpers.h"
#include "net/server/http_server_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace network {

class DialServiceTest : public testing::Test {
 protected:
  base::test::TaskEnvironment scoped_task_env_;
  std::unique_ptr<DialService> service_;

  void TearDown() override { service_.reset(); }

  void GetHandlerTest(
      const std::string& path, const std::string& expected_path,
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

  GetHandlerTest("", "", nullptr);
  GetHandlerTest("/", "", nullptr);
  GetHandlerTest("/apps1/Foo/run", "", nullptr);
  GetHandlerTest("/apps/Bar/foo", "", nullptr);
  GetHandlerTest("/apps/FooBar/", "", nullptr);
  GetHandlerTest("/apps/BarFoo/", "", nullptr);

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
  foo_handler = nullptr;

  service_->Terminate();
}

}  // namespace network
}  // namespace cobalt
