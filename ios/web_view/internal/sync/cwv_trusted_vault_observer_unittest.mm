// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_trusted_vault_observer_internal.h"

#import <Foundation/Foundation.h>

#include "components/sync/driver/trusted_vault_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Concrete observer just for testing.
class TrustedVaultObserver : public syncer::TrustedVaultClient::Observer {
  void OnTrustedVaultKeysChanged() override {}
  void OnTrustedVaultRecoverabilityChanged() override {}
};

using CWVTrustedVaultObserverTest = PlatformTest;

// Tests CWVTrustedVaultObserver initialization.
TEST_F(CWVTrustedVaultObserverTest, Initialization) {
  TrustedVaultObserver observer;
  CWVTrustedVaultObserver* cwv_observer =
      [[CWVTrustedVaultObserver alloc] initWithTrustedVaultObserver:&observer];
  EXPECT_EQ(&observer, cwv_observer.observer);
}

// Tests two CWVTrustedVaultObserver are equal if they wrap the same observer.
TEST_F(CWVTrustedVaultObserverTest, Equal) {
  TrustedVaultObserver observer;
  CWVTrustedVaultObserver* cwv_observer_1 =
      [[CWVTrustedVaultObserver alloc] initWithTrustedVaultObserver:&observer];
  CWVTrustedVaultObserver* cwv_observer_2 =
      [[CWVTrustedVaultObserver alloc] initWithTrustedVaultObserver:&observer];
  EXPECT_NSEQ(cwv_observer_1, cwv_observer_2);
  EXPECT_EQ(cwv_observer_1.hash, cwv_observer_2.hash);
}

// Tests two CWVTrustedVaultObserver are not equal if they wrap different
// observers.
TEST_F(CWVTrustedVaultObserverTest, NotEqual) {
  TrustedVaultObserver observer_1;
  TrustedVaultObserver observer_2;
  CWVTrustedVaultObserver* cwv_observer_1 = [[CWVTrustedVaultObserver alloc]
      initWithTrustedVaultObserver:&observer_1];
  CWVTrustedVaultObserver* cwv_observer_2 = [[CWVTrustedVaultObserver alloc]
      initWithTrustedVaultObserver:&observer_2];
  EXPECT_NSNE(cwv_observer_1, cwv_observer_2);
}

}  // namespace ios_web_view
