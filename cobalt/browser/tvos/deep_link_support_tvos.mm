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

#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"

@implementation DeepLinkSupportTvos

+ (void)handleSiriIntents:(NSString*)query isSearch:(BOOL)isSearch {
  if (query.length == 0) {
    LOG(WARNING) << "AppIntent query is empty, ignoring.";
    return;
  }

  static constexpr NSString* const kYTApplicationURLKey = @"YTApplicationURL";
  NSString* urlBase =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:kYTApplicationURLKey];
  if (!urlBase) {
    LOG(WARNING) << "YTApplicationURL not in plist, ignoring AppIntent.";
    return;
  }

  static constexpr NSString* const kSearchUrlAction =
      @"?launch=voice&vs=11&va=search&vaa=";
  static constexpr NSString* const kPlayUrlAction =
      @"?launch=voice&vs=11&va=play&vaa=";
  NSString* urlAction = isSearch ? kSearchUrlAction : kPlayUrlAction;

  const std::string deep_link = [[NSString
      stringWithFormat:@"%@%@%@", urlBase, urlAction, query] UTF8String];
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  manager->OnDeepLink(deep_link);
}

@end
