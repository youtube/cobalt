// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_SCOPED_GLOBAL_SCENARIO_MEMORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_SCOPED_GLOBAL_SCENARIO_MEMORY_H_

#include <optional>

#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_manager {

// A scoped object that maps in writable shared memory for the global
// performance scenario state as long as it exists. The embedder should create
// one of these on startup that lasts for the lifetime of the browser.
// Performance Manager will update the state, which can be read from any process
// using functions in
// //components/performance_manager/scenario_api/performance_scenarios.h.
class ScopedGlobalScenarioMemory {
 public:
  ScopedGlobalScenarioMemory();
  ~ScopedGlobalScenarioMemory();

  ScopedGlobalScenarioMemory(const ScopedGlobalScenarioMemory&) = delete;
  ScopedGlobalScenarioMemory& operator=(const ScopedGlobalScenarioMemory&) =
      delete;

 private:
  // The browser process also maps in the global scenario memory read-only. The
  // //components/performance_manager/scenario_api/performance_scenarios.h
  // query functions will read from this for ScenarioScope::kGlobal.
  std::optional<performance_scenarios::ScopedReadOnlyScenarioMemory>
      read_only_mapping_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_SCOPED_GLOBAL_SCENARIO_MEMORY_H_
