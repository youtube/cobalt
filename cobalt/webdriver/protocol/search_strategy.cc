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

#include "cobalt/webdriver/protocol/search_strategy.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {

const char kUsingKey[] = "using";
const char kValueKey[] = "value";

struct StrategyStringMapping {
  SearchStrategy::Strategy strategy;
  const char* name;
};
StrategyStringMapping strategy_mapping[] = {
    {SearchStrategy::kClassName, "class name"},
    {SearchStrategy::kCssSelector, "css selector"},
    {SearchStrategy::kId, "id"},
    {SearchStrategy::kName, "name"},
    {SearchStrategy::kLinkText, "link text"},
    {SearchStrategy::kPartialLinkText, "partial link text"},
    {SearchStrategy::kTagName, "tag name"},
    {SearchStrategy::kXPath, "xpath"},
};

bool GetSearchStrategyFromString(const std::string& strategy_string,
                                 SearchStrategy::Strategy* out_strategy) {
  for (size_t i = 0; i < arraysize(strategy_mapping); ++i) {
    if (strategy_mapping[i].name == strategy_string) {
      *out_strategy = strategy_mapping[i].strategy;
      return true;
    }
  }
  return false;
}
}  // namespace

base::Optional<SearchStrategy> SearchStrategy::FromValue(
    const base::Value* value) {
  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value)) {
    return base::nullopt;
  }
  std::string using_strategy;
  std::string parameter;
  if (!dictionary_value->GetString(kUsingKey, &using_strategy)) {
    return base::nullopt;
  }
  if (!dictionary_value->GetString(kValueKey, &parameter)) {
    return base::nullopt;
  }
  SearchStrategy::Strategy strategy;
  if (!GetSearchStrategyFromString(using_strategy, &strategy)) {
    return base::nullopt;
  }
  return SearchStrategy(strategy, parameter);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
