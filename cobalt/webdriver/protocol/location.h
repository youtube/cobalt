// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_PROTOCOL_LOCATION_H_
#define COBALT_WEBDRIVER_PROTOCOL_LOCATION_H_

#include <memory>

#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// A pair of numbers representing the location of an entity.
class Location {
 public:
  static std::unique_ptr<base::Value::Dict> ToValue(const Location& location);

  Location() : x_(0.f), y_(0.f) {}
  Location(float x, float y) : x_(x), y_(y) {}

 private:
  float x_;
  float y_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_LOCATION_H_
