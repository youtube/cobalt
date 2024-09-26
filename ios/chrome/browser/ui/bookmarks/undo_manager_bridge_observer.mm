// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/undo_manager_bridge_observer.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace bookmarks {
UndoManagerBridge::UndoManagerBridge(id<UndoManagerBridgeObserver> observer)
    : observer_(observer) {
  DCHECK(observer);
}

void UndoManagerBridge::OnUndoManagerStateChange() {
  [observer_ undoManagerChanged];
}
}  // namespace bookmarks
