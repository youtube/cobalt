// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_H_

#import "components/keyed_service/core/keyed_service.h"

// KeyedService that manages updating the app-group shared capabilities for
// Gemini/App Switcher integration.
class GeminiCapabilitiesManager : public KeyedService {
 public:
  ~GeminiCapabilitiesManager() override = default;

  // Triggers an update of all app-group capabilities.
  virtual void UpdateCapabilities() = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_H_
