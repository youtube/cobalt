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

#include "cobalt/webdriver/protocol/cookie.h"

#include "base/string_split.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
// JSON Cookie object keys.
const char kNameKey[] = "name";
const char kValueKey[] = "value";
}  // namesace

scoped_ptr<base::Value> Cookie::ToValue(const Cookie& cookie) {
  scoped_ptr<base::DictionaryValue> cookie_value(new base::DictionaryValue());
  cookie_value->SetString(kNameKey, cookie.name_);
  cookie_value->SetString(kValueKey, cookie.value_);
  return cookie_value.PassAs<base::Value>();
}

void Cookie::ToCookieVector(const std::string& cookies_string,
                            std::vector<Cookie>* cookies) {
  std::vector<std::string> cookie_strings;
  base::SplitString(cookies_string, ';', &cookie_strings);
  for (size_t i = 0; i < cookie_strings.size(); ++i) {
    base::optional<Cookie> cookie = FromString(cookie_strings[i]);
    if (cookie) {
      cookies->push_back(*cookie);
    }
  }
}

base::optional<Cookie> Cookie::FromString(const std::string& cookie_as_string) {
  size_t pos = cookie_as_string.find('=');
  if (pos == std::string::npos) {
    return base::nullopt;
  }
  DCHECK(pos < cookie_as_string.size());
  return Cookie(cookie_as_string.substr(0, pos),
                cookie_as_string.substr(pos + 1));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
