// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"),
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

#import <Foundation/Foundation.h>
#include <stdlib.h>

#include "starboard/system.h"

/**
 *  @brief The fallback language locale to use when the system's locale is not
 *      supported.
 */
static NSString* const kFallbackLocaleLanguageString = @"en-US";

const char* SbSystemGetLocaleId() {
  @autoreleasepool {
    static char* localeCString;
    static NSString* localeString;

    static dispatch_once_t pred;
    static NSSet<NSString*>* knownLocales;
    dispatch_once(&pred, ^{
      knownLocales = [NSSet setWithArray:@[
        @"ar",    @"ca",    @"cs",    @"da",     @"de",    @"el", @"en",
        @"en-GB", @"en-US", @"es",    @"es-419", @"fi",    @"fr", @"fr-CA",
        @"he",    @"hi",    @"hr",    @"hu",     @"id",    @"it", @"ja",
        @"ko",    @"ms",    @"ms-bn", @"ms-my",  @"nb",    @"nl", @"nn",
        @"no",    @"pl",    @"pt",    @"pt-BR",  @"pt-PT", @"ro", @"ro-mo",
        @"ru",    @"sk",    @"sv",    @"th",     @"tr",    @"uk", @"vi",
        @"zh",    @"zh-CN", @"zh-TW",
      ]];
    });

    NSString* localeLanguageString = NSLocale.currentLocale.languageCode;

    NSString* selectedLocaleLanguageString;
    for (NSString* preferredLanguage in NSLocale.preferredLanguages) {
      if ([knownLocales containsObject:preferredLanguage]) {
        selectedLocaleLanguageString = preferredLanguage;
        break;
      }
    }

    if (!selectedLocaleLanguageString) {
      if ([knownLocales containsObject:localeLanguageString]) {
        selectedLocaleLanguageString = localeLanguageString;
      } else {
        selectedLocaleLanguageString = kFallbackLocaleLanguageString;
      }
    }

    // Chinese includes variations that are non-region related. Apple appends
    // the current "region" set by the user to the end of the preferredLanguage.
    // So for Traditional Chinese, the preferredLanguage code would be:
    // zh-Hant-US if you're in the US and similar if you're in another region.
    if ([selectedLocaleLanguageString isEqualToString:@"zh"]) {
      NSString* preferredLanguage = NSLocale.preferredLanguages.firstObject;
      if ([preferredLanguage isEqualToString:@"zh-Hant-HK"]) {
        // User selected "Chinese, Traditional (Hong Kong)"
        selectedLocaleLanguageString = @"zh-HK";
      } else if ([preferredLanguage containsString:@"zh-Hant"]) {
        // User selected "Chinese, Traditional"
        selectedLocaleLanguageString = @"zh-TW";
      }
    }

    if (!localeCString ||
        ![selectedLocaleLanguageString isEqual:localeString]) {
      localeString = selectedLocaleLanguageString;
      if (localeCString) {
        free(localeCString);
      }

      NSUInteger length =
          [localeString maximumLengthOfBytesUsingEncoding:NSUTF8StringEncoding];
      length++;  // NULL terminator.

      localeCString = static_cast<char*>(calloc(length, sizeof(char)));
      [localeString getCString:localeCString
                     maxLength:length
                      encoding:NSUTF8StringEncoding];
    }

    return localeCString;
  }
}
