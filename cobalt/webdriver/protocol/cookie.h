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

#ifndef COBALT_WEBDRIVER_PROTOCOL_COOKIE_H_
#define COBALT_WEBDRIVER_PROTOCOL_COOKIE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// https://w3c.github.io/webdriver/webdriver-spec.html#cookies
class Cookie {
 public:
  static scoped_ptr<base::Value> ToValue(const Cookie& cookie);

  // Populates a vector of Cookie objects given a semi-colon separated string
  // of cookies such as returned by Document.cookie.
  static void ToCookieVector(const std::string& cookies_string,
                             std::vector<Cookie>* cookies);

  const std::string& name() const { return name_; }
  const std::string& value() const { return value_; }

 private:
  static base::optional<Cookie> FromString(const std::string& cookie_as_string);
  Cookie(const std::string& name, const std::string& value)
      : name_(name), value_(value) {}
  std::string name_;
  std::string value_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_COOKIE_H_
