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

#ifndef COBALT_NETWORK_DIAL_DIAL_TEST_HELPERS_H_
#define COBALT_NETWORK_DIAL_DIAL_TEST_HELPERS_H_

#include <string>

#include "base/message_loop/message_loop.h"
#include "cobalt/network/dial/dial_service_handler.h"
#include "net/server/http_server_request_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace network {

class MockServiceHandler : public DialServiceHandler {
 public:
  explicit MockServiceHandler(const std::string& service_name)
      : service_name_(service_name) {}
  MOCK_METHOD3(HandleRequest, void(const std::string&,
                                   const net::HttpServerRequestInfo& request,
                                   const CompletionCB& completion_cb));
  const std::string& service_name() const override { return service_name_; }

 private:
  ~MockServiceHandler() {}
  std::string service_name_;
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DIAL_DIAL_TEST_HELPERS_H_
