// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_observer_bridge.h"

GeminiServiceObserverBridge::GeminiServiceObserverBridge(
    id<GeminiServiceObserving> owner,
    GeminiService* service)
    : owner_(owner) {
  scoped_observation_.Observe(service);
}

GeminiServiceObserverBridge::~GeminiServiceObserverBridge() = default;

void GeminiServiceObserverBridge::OnGeminiEligibilityChanged() {
  [owner_ geminiEligibilityDidChange];
}
