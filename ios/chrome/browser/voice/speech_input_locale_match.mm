// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/speech_input_locale_match.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Keys used in SpeechInputLocaleMatches.plist:
NSString* const kMatchedLocaleKey = @"Locale";
NSString* const kMatchingLocalesKey = @"MatchingLocales";
NSString* const kMatchingLanguagesKey = @"MatchingLanguages";

}  // namespace

@implementation SpeechInputLocaleMatch

@synthesize matchedLocale = _matchedLocale;
@synthesize matchingLocales = _matchingLocales;
@synthesize matchingLanguages = _matchingLanguages;

- (instancetype)initWithMatchedLocale:(NSString*)matchedLocale
                      matchingLocales:(NSArray<NSString*>*)matchingLocales
                    matchingLanguages:(NSArray<NSString*>*)matchingLanguages {
  if ((self = [super init])) {
    _matchedLocale = [matchedLocale copy];
    _matchingLocales = [matchingLocales copy];
    _matchingLanguages = [matchingLanguages copy];
  }
  return self;
}

- (instancetype)initWithDictionary:(NSDictionary*)dict {
  NSString* matchedLocale =
      base::mac::ObjCCastStrict<NSString>(dict[kMatchedLocaleKey]);

  NSArray* matchingLocales =
      base::mac::ObjCCastStrict<NSArray>(dict[kMatchingLocalesKey]);
  for (id machingLocale : matchingLocales) {
    DCHECK([machingLocale isKindOfClass:[NSString class]]);
  }

  NSArray* machingLanguages =
      base::mac::ObjCCastStrict<NSArray>(dict[kMatchingLanguagesKey]);
  for (id machingLanguage : machingLanguages) {
    DCHECK([machingLanguage isKindOfClass:[NSString class]]);
  }

  return [self initWithMatchedLocale:matchedLocale
                     matchingLocales:matchingLocales
                   matchingLanguages:machingLanguages];
}

@end

NSArray<SpeechInputLocaleMatch*>* LoadSpeechInputLocaleMatches() {
  NSString* path =
      [[NSBundle mainBundle] pathForResource:@"SpeechInputLocaleMatches"
                                      ofType:@"plist"
                                 inDirectory:@"gm-config/ANY"];

  NSMutableArray<SpeechInputLocaleMatch*>* matches = [NSMutableArray array];
  for (id item in [NSArray arrayWithContentsOfFile:path]) {
    NSDictionary* dict = base::mac::ObjCCastStrict<NSDictionary>(item);
    SpeechInputLocaleMatch* match =
        [[SpeechInputLocaleMatch alloc] initWithDictionary:dict];
    [matches addObject:match];
  }
  return matches;
}
