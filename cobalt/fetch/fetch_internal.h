// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_FETCH_FETCH_INTERNAL_H_
#define COBALT_FETCH_FETCH_INTERNAL_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace fetch {

// Wrapper for utility functions for use with the fetch polyfill. This is
// specific to the fetch polyfill and may change as the implementation changes.
// This is not meant to be public and should not be used outside of the fetch
// implementation.
class FetchInternal : public script::Wrappable {
 public:
  // Return whether the given URL is valid.
  static bool IsUrlValid(const std::string& url);

  DEFINE_WRAPPABLE_TYPE(FetchInternal);
};

}  // namespace fetch
}  // namespace cobalt

#endif  // COBALT_FETCH_FETCH_INTERNAL_H_
