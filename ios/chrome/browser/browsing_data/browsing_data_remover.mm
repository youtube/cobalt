// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_remover.h"

#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowsingDataRemover::BrowsingDataRemover() = default;

BrowsingDataRemover::~BrowsingDataRemover() = default;

void BrowsingDataRemover::AddObserver(BrowsingDataRemoverObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowsingDataRemover::RemoveObserver(
    BrowsingDataRemoverObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowsingDataRemover::NotifyBrowsingDataRemoved(
    BrowsingDataRemoveMask mask) {
  for (BrowsingDataRemoverObserver& observer : observers_) {
    observer.OnBrowsingDataRemoved(this, mask);
  }
}
