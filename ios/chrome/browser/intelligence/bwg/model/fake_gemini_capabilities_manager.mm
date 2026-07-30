// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_capabilities_manager.h"

FakeGeminiCapabilitiesManager::FakeGeminiCapabilitiesManager() = default;
FakeGeminiCapabilitiesManager::~FakeGeminiCapabilitiesManager() = default;

void FakeGeminiCapabilitiesManager::Shutdown() {}

void FakeGeminiCapabilitiesManager::UpdateCapabilities() {
  update_capabilities_count_++;
}
