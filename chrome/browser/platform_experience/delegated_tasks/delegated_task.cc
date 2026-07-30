// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/time/time.h"

namespace platform_experience {

namespace {

constexpr auto kTaskNameToTypeMap =
    base::MakeFixedFlatMap<std::string_view, DelegatedTaskType>({
        {
            "RegisterSearchPromotion",
            DelegatedTaskType::kRegisterSearchPromotion,
        },
    });

}  // namespace

base::TimeDelta DelegatedTask::GetTimeout() const {
  return base::Seconds(10);
}

void DelegatedTask::AppendCommandLineSwitches(
    base::CommandLine& /*cmd_line*/) const {}

std::optional<DelegatedTaskType> ParseDelegatedTaskType(
    std::string_view task_name) {
  const auto it = kTaskNameToTypeMap.find(task_name);
  return it != kTaskNameToTypeMap.end() ? std::make_optional(it->second)
                                        : std::nullopt;
}

}  // namespace platform_experience
