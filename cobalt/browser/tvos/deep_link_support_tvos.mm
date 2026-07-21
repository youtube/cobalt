// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#import "cobalt/browser/tvos/deep_link_support_tvos.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "cobalt/browser/tvos/bundle_data.h"

@implementation DeepLinkSupportTvos

+ (void)handleSiriIntents:(NSString*)query isSearch:(BOOL)isSearch {
  if (query.length == 0) {
    LOG(WARNING) << "AppIntent query is empty, ignoring.";
    return;
  }

  const auto urlBase = cobalt::GetValueFromPlistAsString("YTApplicationURL");
  if (!urlBase) {
    LOG(WARNING) << "YTApplicationURL not in plist, ignoring AppIntent.";
    return;
  }

  static constexpr std::string_view kSearchUrlAction =
      "?launch=voice&vs=11&va=search&vaa=";
  static constexpr std::string_view kPlayUrlAction =
      "?launch=voice&vs=11&va=play&vaa=";
  const std::string_view urlAction =
      isSearch ? kSearchUrlAction : kPlayUrlAction;

  const std::string deep_link =
      base::StrCat({*urlBase, urlAction, base::SysNSStringToUTF8(query)});
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  manager->OnDeepLink(deep_link);
}

@end
