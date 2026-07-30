// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class WebState;
}  // namespace web

class Browser;

// `ActorOverlayCoordinator` is a short-lived coordinator launched when a
// tab is actively being actuated by the actor. It manages the actor overlay
// components over the base view controller.
//
// There is only one `ActorOverlayCoordinator` per `Browser`.
@interface ActorOverlayCoordinator : ChromeCoordinator

// Initializes the coordinator with a base view controller, a browser instance,
// and the web state undergoing actuation. This method must be called on the
// main thread.
//
// `viewController` is the base view controller used to present the actuation
// UI.
// `browser` is the browser instance containing the tab.
// `webState` is the web state of the tab being actuated.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
