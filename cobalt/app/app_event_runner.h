// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_APP_APP_EVENT_RUNNER_H_
#define COBALT_APP_APP_EVENT_RUNNER_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "content/public/app/content_main.h"
#include "starboard/event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {

// AppEventRunner defines an interface for managing the system-level
// execution of the Cobalt process. This abstraction allows for injecting
// fake or mock behaviors during unit testing.
//
// AppEventRunner manages the Cobalt application lifecycle by translating
// Starboard events into Chromium actions. It orchestrates the initialization,
// running, and shutdown of the browser process and manages interaction with
// Cobalt subsystems such as DeepLinkManager and H5vccAccessibilityManager.
class AppEventRunner {
 public:
  static std::unique_ptr<AppEventRunner> Create();

  virtual ~AppEventRunner() = default;

  // Initializes global system resources like AtExitManager.
  virtual void InitializeSystem() = 0;

  // Creates the main delegate for the content process.
  virtual void CreateMainDelegate(absl::optional<int64_t> startup_timestamp,
                                  bool is_visible,
                                  const char* initial_deep_link) = 0;

  // Returns the main delegate.
  virtual cobalt::CobaltMainDelegate* GetMainDelegate() = 0;

  // Lifecycle signals called from the application.
  // These functions check and update the running, visible, focused, and frozen
  // state values and call their matching DoFoo() functions for the actual
  // transition work.
  void OnStart(const SbEvent* event);
  void OnStop();
  void OnBlur();
  void OnFocus();
  void OnConceal();
  void OnReveal();
  void OnFreeze();
  void OnUnfreeze();

  // Handlers for non-lifecycle SbEventType events
  virtual void OnInput(const SbEvent* event) = 0;
  virtual void OnLink(const SbEvent* event) = 0;
  virtual void OnLowMemory(const SbEvent* event) = 0;
  virtual void OnWindowSizeChanged(const SbEvent* event) = 0;
  virtual void OnOsNetworkConnectedDisconnected(const SbEvent* event) = 0;
  virtual void OnDateTimeConfigurationChanged(const SbEvent* event) = 0;
  virtual void OnAccessibilityTextToSpeechSettingsChanged(
      const SbEvent* event) = 0;

  // State tracking
  bool is_running() const { return is_running_.load(); }
  bool is_visible() const { return is_visible_.load(); }
  bool is_focused() const { return is_focused_.load(); }
  bool is_frozen() const { return is_frozen_.load(); }

  void set_is_running(bool value) { is_running_.store(value); }
  void set_is_visible(bool value) { is_visible_.store(value); }
  void set_is_focused(bool value) { is_focused_.store(value); }
  void set_is_frozen(bool value) { is_frozen_.store(value); }

 private:
  // Lifecycle signals called from the application.
  virtual void DoStart(const SbEvent* event) = 0;
  virtual void DoStop() = 0;
  virtual void DoBlur() = 0;
  virtual void DoFocus() = 0;
  virtual void DoConceal() = 0;
  virtual void DoReveal() = 0;
  virtual void DoFreeze() = 0;
  virtual void DoUnfreeze() = 0;

  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_visible_{false};
  std::atomic<bool> is_focused_{false};
  std::atomic<bool> is_frozen_{true};
};

}  // namespace cobalt

#endif  // COBALT_APP_APP_EVENT_RUNNER_H_
