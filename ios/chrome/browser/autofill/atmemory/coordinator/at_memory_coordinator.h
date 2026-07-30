// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for AtMemory feature.
@interface AtMemoryCoordinator
    : ChromeCoordinator <UIAdaptivePresentationControllerDelegate>

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_COORDINATOR_H_
