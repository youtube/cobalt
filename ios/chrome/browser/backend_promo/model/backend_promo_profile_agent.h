// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_PROFILE_AGENT_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

// A profile agent that initializes the BackendPromoService when the profile is
// initialized, and notifies it when the app becomes foreground active.
@interface BackendPromoProfileAgent : SceneObservingProfileAgent

@end

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_PROFILE_AGENT_H_
