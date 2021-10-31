// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSS_PARSER_STRING_POOL_H_
#define COBALT_CSS_PARSER_STRING_POOL_H_

#include <memory>
#include <string>


namespace cobalt {
namespace css_parser {

// Manages strings created by a scanner. Those are strings that had to be
// modified because they contained an escape sequence.
class StringPool {
 public:
  StringPool() {}
  ~StringPool() {}

  // Returns a new string. The string will be deallocated along with the pool.
  std::string* AllocateString();

 private:
  std::vector<std::unique_ptr<std::string>> strings_;

  DISALLOW_COPY_AND_ASSIGN(StringPool);
};

inline std::string* StringPool::AllocateString() {
  strings_.emplace_back(new std::string());
  return strings_.back().get();
}

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_STRING_POOL_H_
