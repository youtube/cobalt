// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_MODALITY_TEST_PRESENTED_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_MODALITY_TEST_PRESENTED_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"

// Coordinator to use in tests for overlay UI supported using presented
// UIViewControllers.
@interface TestPresentedOverlayCoordinator : OverlayRequestCoordinator
@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_MODALITY_TEST_PRESENTED_OVERLAY_COORDINATOR_H_
