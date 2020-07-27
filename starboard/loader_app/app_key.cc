// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/app_key.h"

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/loader_app/app_key_internal.h"

namespace starboard {
namespace loader_app {
namespace {

// With the maximum length of an app key being 24 bytes less than the maximum
// file name size we provide enough room for not-insignificant sized prefixes
// and suffixes. This leaves room for prefixes and suffixes while maintaining
// all, or most of, the app key.
const size_t kAppKeyMax = kSbFileMaxName - 24;

}  // namespace

std::string GetAppKey(const std::string& url) {
  SB_DCHECK(kAppKeyMax > 0);

  const std::string app_key = EncodeAppKey(ExtractAppKey(url));

  if (app_key.size() > kAppKeyMax) {
    return app_key.substr(app_key.size() - kAppKeyMax, kAppKeyMax);
  }
  return app_key;
}

}  // namespace loader_app
}  // namespace starboard
