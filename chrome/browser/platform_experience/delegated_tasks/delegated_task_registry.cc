// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_registry.h"

#include "base/notreached.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"

namespace platform_experience {

namespace {

constexpr const char* kRegisterSearchPromotionKeys[] = {"post-install-url"};

}  // namespace

DelegatedTaskMetadata GetDelegatedTaskMetadata(DelegatedTaskType type) {
  switch (type) {
    case DelegatedTaskType::kRegisterSearchPromotion:
      return {
          .task_name = "RegisterSearchPromotion",
          .cmdline_switch_keys = kRegisterSearchPromotionKeys,
      };
    default:
      NOTREACHED();
  }
}

}  // namespace platform_experience
