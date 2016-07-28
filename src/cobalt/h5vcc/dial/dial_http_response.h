/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_H5VCC_DIAL_DIAL_HTTP_RESPONSE_H_
#define COBALT_H5VCC_DIAL_DIAL_HTTP_RESPONSE_H_

#include <string>
#include <vector>

#include "cobalt/script/wrappable.h"
#include "net/dial/dial_service_handler.h"

namespace cobalt {
namespace h5vcc {
namespace dial {

class DialHttpResponse : public script::Wrappable {
 public:
  DialHttpResponse(const std::string& path, const std::string& method);

  std::string path() const { return path_; }
  std::string method() const { return method_; }
  uint16 response_code() const { return response_code_; }
  std::string body() const { return body_; }
  std::string mime_type() const { return mime_type_; }

  void set_response_code(uint16 response_code) {
    response_code_ = response_code;
  }
  void set_body(const std::string& body) { body_ = body; }
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

  void AddHeader(const std::string& header, const std::string& value);

  scoped_ptr<net::HttpServerResponseInfo> ToHttpServerResponseInfo() const;

  DEFINE_WRAPPABLE_TYPE(DialHttpResponse);

 private:
  std::string path_;
  std::string method_;
  uint16 response_code_;
  std::string body_;
  std::string mime_type_;

  std::vector<std::string> headers_;

  DISALLOW_COPY_AND_ASSIGN(DialHttpResponse);
};

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_DIAL_DIAL_HTTP_RESPONSE_H_
