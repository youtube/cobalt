// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_context_data.h"

namespace extensions {

std::unique_ptr<ContextData> TestContextData::Clone() const {
  return std::make_unique<TestContextData>();
}

bool TestContextData::IsIsolatedApplication() const {
  return false;
}

}  // namespace extensions
