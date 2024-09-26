// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/sync_observer_bridge.h"

#import "base/check.h"
#import "components/sync/driver/sync_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SyncObserverBridge::SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                                       syncer::SyncService* sync_service)
    : delegate_(delegate) {
  DCHECK(delegate);
  if (sync_service)
    scoped_observation_.Observe(sync_service);
}

SyncObserverBridge::~SyncObserverBridge() {}

void SyncObserverBridge::OnStateChanged(syncer::SyncService* sync) {
  [delegate_ onSyncStateChanged];
}

void SyncObserverBridge::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  if ([delegate_ respondsToSelector:@selector(onSyncConfigurationCompleted)])
    [delegate_ onSyncConfigurationCompleted];
}
