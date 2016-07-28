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
#include "base/time.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// https://w3c.github.io/webdriver/webdriver-spec.html#cookies
class Cookie {
 public:
  static scoped_ptr<base::Value> ToValue(const Cookie& cookie);
  static base::optional<Cookie> FromValue(const base::Value* value);

  // Populates a vector of Cookie objects given a semi-colon separated string
  // of cookies such as returned by Document.cookie.
  static void ToCookieVector(const std::string& cookies_string,
                             std::vector<Cookie>* cookies);

  const std::string& name() const { return name_; }
  const std::string& value() const { return value_; }
  const base::optional<std::string>& domain() const { return domain_; }
  const base::optional<std::string>& path() const { return path_; }
  const base::optional<bool>& secure() const { return secure_; }
  const base::optional<bool>& http_only() const { return http_only_; }
  const base::optional<base::Time>& expiry_time() const { return expiry_time_; }

  // Constructs and returns a cookie string that could be passed to
  // Document.cookie, e.g. "foo=var; Path=/; Secure"
  std::string ToCookieString(const std::string& current_domain) const;

 private:
  static base::optional<Cookie> FromString(const std::string& cookie_as_string);
  Cookie(const std::string& name, const std::string& value)
      : name_(name), value_(value) {}
  std::string name_;
  std::string value_;
  base::optional<std::string> path_;
  base::optional<std::string> domain_;
  base::optional<bool> secure_;
  base::optional<bool> http_only_;
  base::optional<base::Time> expiry_time_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_COOKIE_H_
