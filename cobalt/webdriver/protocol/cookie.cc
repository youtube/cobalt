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

#include "cobalt/webdriver/protocol/cookie.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
// The key for the JSON Cookie object in the parameters.
const char kCookieKey[] = "cookie";

// JSON Cookie object keys.
const char kNameKey[] = "name";
const char kValueKey[] = "value";
const char kDomainKey[] = "domain";
const char kPathKey[] = "path";
const char kSecureKey[] = "secure";
const char kHttpOnlyKey[] = "httpOnly";
const char kExpiryKey[] = "expiry";
}  // namespace

std::unique_ptr<base::Value> Cookie::ToValue(const Cookie& cookie) {
  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* cookie_value = ret.GetIfDict();
  cookie_value->Set(kNameKey, cookie.name_);
  cookie_value->Set(kValueKey, cookie.value_);
  return base::Value::ToUniquePtrValue(std::move(ret));
}

base::Optional<Cookie> Cookie::FromValue(const base::Value* value) {
  // TODO: Malformed data should return an "unable to set cookie"
  // error, but the current implementation will return "invalid parameter".
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    LOG(ERROR) << "Parameter is not a dictionary.";
    return base::nullopt;
  }

  const base::Value::Dict* cookie_dictionary_value =
      dictionary_value->FindDict(kCookieKey);
  if (!cookie_dictionary_value) {
    LOG(ERROR) << base::StringPrintf("Value of key [%s] is not a JSON object.",
                                     kCookieKey);
    return base::nullopt;
  }

  const std::string* cookie_name =
      cookie_dictionary_value->FindString(kNameKey);
  const std::string* cookie_value =
      cookie_dictionary_value->FindString(kValueKey);
  // Name and value are required.
  if (!cookie_name || !cookie_value) {
    LOG(ERROR) << base::StringPrintf(
        "cookie.%s or cookie.%s either does not exist or is not a string",
        kNameKey, kValueKey);
    return base::nullopt;
  }

  Cookie new_cookie(*cookie_name, *cookie_value);

  const std::string* domain_value =
      cookie_dictionary_value->FindString(kDomainKey);
  if (domain_value) {
    new_cookie.domain_ = *domain_value;
  } else if (cookie_dictionary_value->contains(kDomainKey)) {
    LOG(ERROR) << base::StringPrintf("cookie.%s is not a string", kDomainKey);
    return base::nullopt;
  }

  const std::string* path_value = cookie_dictionary_value->FindString(kPathKey);
  if (path_value) {
    new_cookie.path_ = *path_value;
  } else if (cookie_dictionary_value->contains(kPathKey)) {
    LOG(ERROR) << base::StringPrintf("cookie.%s is not a string", kPathKey);
    return base::nullopt;
  }

  base::Optional<bool> secure_value =
      cookie_dictionary_value->FindBool(kSecureKey);
  if (secure_value.has_value()) {
    new_cookie.secure_ = secure_value.value();
  } else if (cookie_dictionary_value->contains(kSecureKey)) {
    LOG(ERROR) << base::StringPrintf("cookie.%s is not a boolean", kSecureKey);
    return base::nullopt;
  }

  base::Optional<bool> http_only_value =
      cookie_dictionary_value->FindBool(kHttpOnlyKey);
  if (http_only_value.has_value()) {
    new_cookie.http_only_ = http_only_value.value();
  } else if (cookie_dictionary_value->contains(kHttpOnlyKey)) {
    LOG(ERROR) << base::StringPrintf("cookie.%s is not a boolean",
                                     kHttpOnlyKey);
    return base::nullopt;
  }

  base::Optional<int> expiry_value =
      cookie_dictionary_value->FindInt(kExpiryKey);
  if (expiry_value.has_value()) {
    base::TimeDelta seconds_since_epoch = base::Seconds(expiry_value.value());
    new_cookie.expiry_time_ = base::Time::UnixEpoch() + seconds_since_epoch;
  } else if (cookie_dictionary_value->contains(kExpiryKey)) {
    LOG(ERROR) << base::StringPrintf("cookie.%s is not an integer", kExpiryKey);
    return base::nullopt;
  }
  return new_cookie;
}

void Cookie::ToCookieVector(const std::string& cookies_string,
                            std::vector<Cookie>* cookies) {
  std::vector<std::string> cookie_strings = base::SplitString(
      cookies_string, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < cookie_strings.size(); ++i) {
    base::Optional<Cookie> cookie = FromString(cookie_strings[i]);
    if (cookie) {
      cookies->push_back(*cookie);
    }
  }
}

base::Optional<Cookie> Cookie::FromString(const std::string& cookie_as_string) {
  size_t pos = cookie_as_string.find('=');
  if (pos == std::string::npos) {
    return base::nullopt;
  }
  DCHECK(pos < cookie_as_string.size());
  return Cookie(cookie_as_string.substr(0, pos),
                cookie_as_string.substr(pos + 1));
}

std::string Cookie::ToCookieString(const std::string& current_domain) const {
  base::Time expiry_time;
  if (expiry_time_) {
    expiry_time = expiry_time_.value();
  } else {
    // If expiry was not set by the client, then the default is 20 years from
    // now.
    base::Time::Exploded exploded_now;
    base::Time::Now().UTCExplode(&exploded_now);
    exploded_now.year += 20;
    bool ret = base::Time::FromUTCExploded(exploded_now, &expiry_time);
    DCHECK(ret);
  }

  // WebDriver protocol defines expiry as seconds since the Unix Epoch, but
  // the Max-Age attribute defines it as seconds from right now.
  base::TimeDelta max_age = expiry_time - base::Time::Now();

  std::string cookie_string = base::StringPrintf(
      "%s=%s; Path=%s; Domain=%s; Max-Age=%d%s%s", name_.c_str(),
      value_.c_str(), path_.value_or("/").c_str(),
      domain_.value_or(current_domain).c_str(),
      static_cast<int>(max_age.InSeconds()),
      secure_.value_or(false) ? " ;Secure" : "",
      http_only_.value_or(false) ? " ;HttpOnly" : "");
  return cookie_string;
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
