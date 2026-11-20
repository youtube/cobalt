// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_CAPABILITIES_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_CAPABILITIES_MANAGER_H_

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager.h"

// Fake implementation of GeminiCapabilitiesManager for testing.
class FakeGeminiCapabilitiesManager : public GeminiCapabilitiesManager {
 public:
  FakeGeminiCapabilitiesManager();
  ~FakeGeminiCapabilitiesManager() override;

  // KeyedService implementation.
  void Shutdown() override;

  // GeminiCapabilitiesManager implementation.
  void UpdateCapabilities() override;

  // Helper to count the number of times UpdateCapabilities was called.
  int update_capabilities_count() const { return update_capabilities_count_; }

 private:
  int update_capabilities_count_ = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_CAPABILITIES_MANAGER_H_
