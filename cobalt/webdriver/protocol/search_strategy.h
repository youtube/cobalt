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

#ifndef COBALT_WEBDRIVER_PROTOCOL_SEARCH_STRATEGY_H_
#define COBALT_WEBDRIVER_PROTOCOL_SEARCH_STRATEGY_H_

#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Strategy for finding elements in a page.
class SearchStrategy {
 public:
  enum Strategy {
    kClassName,
    kCssSelector,
    kId,
    kName,
    kLinkText,
    kPartialLinkText,
    kTagName,
    kXPath
  };
  static base::Optional<SearchStrategy> FromValue(const base::Value* value);

  Strategy strategy() const { return strategy_; }
  const std::string parameter() const { return parameter_; }

 private:
  SearchStrategy(Strategy strategy, const std::string& parameter)
      : strategy_(strategy), parameter_(parameter) {}

  Strategy strategy_;
  std::string parameter_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_SEARCH_STRATEGY_H_
