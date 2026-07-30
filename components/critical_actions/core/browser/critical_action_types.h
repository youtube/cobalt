// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_

#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace critical_actions {

// Enum defining list of critical categories supported by Critical Action
// History.
enum class ActionType {
  kUnknown = 0,
  kFormFill = 1,
  kDownload = 2,
  kSettingChange = 3,
  kCredentialAccess = 4,
};

// Represents a memory row copy of a single record in CriticalActions.db.
struct CriticalActionEntry {
  CriticalActionEntry();
  CriticalActionEntry(const CriticalActionEntry&);
  CriticalActionEntry(CriticalActionEntry&&) noexcept;
  CriticalActionEntry& operator=(const CriticalActionEntry&);
  CriticalActionEntry& operator=(CriticalActionEntry&&) noexcept;
  ~CriticalActionEntry();

  std::string critical_action_id;  // Client-generated UUID
  base::Time timestamp;
  int64_t visit_id = 0;         // References History visit
  std::string conversation_id;  // References conversation context
  std::string actor_task_id;          // References agent task
  ActionType action_type = ActionType::kUnknown;
  GURL url;
  std::string metadata;  // Action-specific details in JSON format

  bool operator==(const CriticalActionEntry& other) const = default;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_
