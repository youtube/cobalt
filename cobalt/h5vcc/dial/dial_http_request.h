// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_DIAL_DIAL_HTTP_REQUEST_H_
#define COBALT_H5VCC_DIAL_DIAL_HTTP_REQUEST_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {
namespace dial {
class DialHttpRequest : public script::Wrappable {
 public:
  DialHttpRequest(const std::string& path, const std::string& method,
                  const std::string& body, const std::string& host);

  std::string path() const { return path_; }
  std::string method() const { return method_; }
  std::string body() const { return body_; }
  std::string host() const { return host_; }

  DEFINE_WRAPPABLE_TYPE(DialHttpRequest);

 private:
  std::string path_;
  std::string method_;
  std::string body_;
  std::string host_;

  DISALLOW_COPY_AND_ASSIGN(DialHttpRequest);
};

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_DIAL_DIAL_HTTP_REQUEST_H_
