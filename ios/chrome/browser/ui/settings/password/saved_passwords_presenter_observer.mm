// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SavedPasswordsPresenterObserverBridge::SavedPasswordsPresenterObserverBridge(
    id<SavedPasswordsPresenterObserver> delegate,
    password_manager::SavedPasswordsPresenter* presenter)
    : delegate_(delegate) {
  DCHECK(presenter);
  saved_passwords_presenter_observer_.Observe(presenter);
}

SavedPasswordsPresenterObserverBridge::
    ~SavedPasswordsPresenterObserverBridge() = default;

void SavedPasswordsPresenterObserverBridge::OnSavedPasswordsChanged() {
  [delegate_ savedPasswordsDidChange];
}
