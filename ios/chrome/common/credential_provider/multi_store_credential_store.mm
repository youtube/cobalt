// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/common/credential_provider/credential.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MultiStoreCredentialStore ()

@property(nonatomic, strong) NSArray<id<CredentialStore>>* stores;

@end

@implementation MultiStoreCredentialStore

- (instancetype)initWithStores:(NSArray<id<CredentialStore>>*)stores {
  DCHECK(stores);
  self = [super init];
  if (self) {
    _stores = stores;
  }
  return self;
}

#pragma mark - CredentialStore

- (NSArray<id<Credential>>*)credentials {
  return
      [self.stores valueForKeyPath:@"credentials.@distinctUnionOfArrays.self"];
}

- (id<Credential>)credentialWithRecordIdentifier:(NSString*)recordIdentifier {
  DCHECK(recordIdentifier.length);
  for (id<CredentialStore> store in self.stores) {
    id<Credential> credential =
        [store credentialWithRecordIdentifier:recordIdentifier];
    if (credential) {
      return credential;
    }
  }
  return nil;
}

@end
