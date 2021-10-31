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

#include <memory>

#include "cobalt/webdriver/protocol/cookie.h"

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
  std::unique_ptr<base::DictionaryValue> cookie_value(
      new base::DictionaryValue());
  cookie_value->SetString(kNameKey, cookie.name_);
  cookie_value->SetString(kValueKey, cookie.value_);
  return std::unique_ptr<base::Value>(cookie_value.release());
}

base::Optional<Cookie> Cookie::FromValue(const base::Value* value) {
  // TODO: Malformed data should return an "unable to set cookie"
  // error, but the current implementation will return "invalid parameter".
  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value)) {
    DLOG(INFO) << "Parameter is not a dictionary.";
    return base::nullopt;
  }

  const base::DictionaryValue* cookie_dictionary_value;
  if (!dictionary_value->GetDictionary(kCookieKey, &cookie_dictionary_value)) {
    DLOG(INFO) << base::StringPrintf("Value of key [%s] is not a JSON object.",
                                     kCookieKey);
    return base::nullopt;
  }

  std::string cookie_name;
  std::string cookie_value;
  // Name and value are required.
  if (!cookie_dictionary_value->GetString(kNameKey, &cookie_name) ||
      !cookie_dictionary_value->GetString(kValueKey, &cookie_value)) {
    DLOG(INFO) << base::StringPrintf(
        "cookie.%s or cookie.%s either does not exist or is not a string",
        kNameKey, kValueKey);
    return base::nullopt;
  }

  Cookie new_cookie(cookie_name, cookie_value);

  std::string string_value;
  if (cookie_dictionary_value->HasKey(kDomainKey)) {
    if (cookie_dictionary_value->GetString(kDomainKey, &string_value)) {
      new_cookie.domain_ = string_value;
    } else {
      DLOG(INFO) << base::StringPrintf("cookie.%s is not a string", kDomainKey);
      return base::nullopt;
    }
  }
  if (cookie_dictionary_value->HasKey(kPathKey)) {
    if (cookie_dictionary_value->GetString(kPathKey, &string_value)) {
      new_cookie.path_ = string_value;
    } else {
      DLOG(INFO) << base::StringPrintf("cookie.%s is not a string", kPathKey);
      return base::nullopt;
    }
  }

  bool bool_value = false;
  if (cookie_dictionary_value->HasKey(kSecureKey)) {
    if (cookie_dictionary_value->GetBoolean(kSecureKey, &bool_value)) {
      new_cookie.secure_ = bool_value;
    } else {
      DLOG(INFO) << base::StringPrintf("cookie.%s is not a boolean",
                                       kSecureKey);
      return base::nullopt;
    }
  }
  if (cookie_dictionary_value->HasKey(kHttpOnlyKey)) {
    if (cookie_dictionary_value->GetBoolean(kHttpOnlyKey, &bool_value)) {
      new_cookie.http_only_ = bool_value;
    } else {
      DLOG(INFO) << base::StringPrintf("cookie.%s is not a boolean",
                                       kHttpOnlyKey);
      return base::nullopt;
    }
  }

  int timestamp_value = 0;
  if (cookie_dictionary_value->HasKey(kExpiryKey)) {
    if (cookie_dictionary_value->GetInteger(kExpiryKey, &timestamp_value)) {
      base::TimeDelta seconds_since_epoch =
          base::TimeDelta::FromSeconds(timestamp_value);
      new_cookie.expiry_time_ = base::Time::UnixEpoch() + seconds_since_epoch;
    } else {
      DLOG(INFO) << base::StringPrintf("cookie.%s is not an integer",
                                       kExpiryKey);
      return base::nullopt;
    }
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
