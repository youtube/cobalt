// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "google_api_keys_mac.h"

#import <Foundation/Foundation.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace google_apis {

std::string GetAPIKeyFromInfoPlist(const std::string& key_name) {
  NSString* keyName = base::SysUTF8ToNSString(key_name);
  NSString* keyValue = base::mac::ObjCCast<NSString>(
      [base::mac::FrameworkBundle() objectForInfoDictionaryKey:keyName]);
  return base::SysNSStringToUTF8(keyValue);
}

}  // namespace google_apis
