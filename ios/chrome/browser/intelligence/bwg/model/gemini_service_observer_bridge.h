// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"

// Objective-C protocol to be implemented by classes that want to observe
// GeminiService events.
@protocol GeminiServiceObserving <NSObject>

// Invoked when Gemini eligibility or policy availability changes.
- (void)geminiEligibilityDidChange;

@end

// Bridge class to observe GeminiService in Objective-C.
class GeminiServiceObserverBridge : public GeminiService::Observer {
 public:
  GeminiServiceObserverBridge(id<GeminiServiceObserving> owner,
                              GeminiService* service);
  ~GeminiServiceObserverBridge() override;

  // GeminiService::Observer overrides:
  void OnGeminiEligibilityChanged() override;

 private:
  __weak id<GeminiServiceObserving> owner_ = nil;
  base::ScopedObservation<GeminiService, GeminiService::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_OBSERVER_BRIDGE_H_
