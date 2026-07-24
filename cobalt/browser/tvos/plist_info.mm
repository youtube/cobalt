// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/tvos/plist_info.h"

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace cobalt {

std::optional<std::string> GetValueFromPlistAsString(
    std::string_view key_name) {
  NSString* keyName = base::SysUTF8ToNSString(key_name);
  NSString* keyValue = base::apple::ObjCCast<NSString>(
      [[NSBundle mainBundle] objectForInfoDictionaryKey:keyName]);
  if (!keyValue) {
    return std::nullopt;
  }
  return base::SysNSStringToUTF8(keyValue);
}

}  // namespace cobalt
