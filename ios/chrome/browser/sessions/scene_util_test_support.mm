// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/scene_util_test_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeSceneSession : NSObject

- (instancetype)initWithIdentifier:(NSString*)identifier;

@property(nonatomic, strong, readonly) NSString* persistentIdentifier;

@end

@implementation FakeSceneSession {
  __strong NSString* _identifier;
}

- (instancetype)initWithIdentifier:(NSString*)identifier {
  if ((self = [super init])) {
    _identifier = [identifier copy];
  }
  return self;
}

- (NSString*)persistentIdentifier {
  return _identifier;
}

@end

@interface FakeScene : NSObject

- (instancetype)initWithSession:(id)session;

@property(nonatomic, strong, readonly) FakeSceneSession* session;

@property(nonatomic, strong, readonly) NSArray<UIWindow*>* windows;

@end

@implementation FakeScene {
  __strong FakeSceneSession* _session;
}

- (instancetype)initWithSession:(FakeSceneSession*)session {
  if ((self = [super init])) {
    _session = session;
  }
  return self;
}

- (FakeSceneSession*)session {
  return _session;
}

- (NSArray<UIWindow*>*)windows {
  return nil;
}

@end

id FakeSceneWithIdentifier(NSString* identifier) {
  return [[FakeScene alloc]
      initWithSession:[[FakeSceneSession alloc] initWithIdentifier:identifier]];
}
