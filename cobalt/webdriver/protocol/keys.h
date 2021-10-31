// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_PROTOCOL_KEYS_H_
#define COBALT_WEBDRIVER_PROTOCOL_KEYS_H_

#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Wraps a utf8 string representing a sequence of key inputs.
class Keys {
 public:
  static base::Optional<Keys> FromValue(const base::Value* value);

  const std::string& utf8_keys() const { return utf8_keys_; }

 private:
  explicit Keys(const std::string& utf8_key_string)
      : utf8_keys_(utf8_key_string) {}
  std::string utf8_keys_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_KEYS_H_
