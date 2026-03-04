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

#ifndef COBALT_APP_APP_LIFECYCLE_DELEGATE_H_
#define COBALT_APP_APP_LIFECYCLE_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "content/public/app/content_main.h"
#include "starboard/event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_STARBOARD)
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#endif

namespace cobalt {

// AppLifecycleRunner defines an interface for managing the system-level
// execution of the Cobalt process. This abstraction allows for injecting
// fake or mock behaviors during unit testing.
class AppLifecycleRunner {
 public:
  virtual ~AppLifecycleRunner() = default;

  // Initializes global system resources like AtExitManager.
  virtual void InitializeSystem() = 0;

  // Creates the main delegate for the content process.
  virtual void CreateMainDelegate(absl::optional<int64_t> startup_timestamp,
                                  bool is_visible) = 0;

  // Returns the main delegate.
  virtual cobalt::CobaltMainDelegate* GetMainDelegate() = 0;

  // Executes the content process.
  virtual int Run(content::ContentMainParams params) = 0;

  // Shuts down the runner and its managed resources.
  virtual void ShutDown() = 0;

  // Returns true if the runner is currently executing.
  virtual bool IsRunning() const = 0;
};

// AppLifecycleDelegate manages the Cobalt application lifecycle by translating
// Starboard events into Chromium actions. It orchestrates the initialization,
// running, and shutdown of the browser process and manages interaction with
// Cobalt subsystems such as DeepLinkManager and H5vccAccessibilityManager.
class AppLifecycleDelegate {
 public:
  explicit AppLifecycleDelegate(
      std::unique_ptr<AppLifecycleRunner> runner = nullptr);
  ~AppLifecycleDelegate();

  void HandleEvent(const SbEvent* event);

  bool IsRunning() const;

 private:
  void OnStart(const SbEvent* event);
  void OnStop(const SbEvent* event);

  int Run(absl::optional<int64_t> startup_timestamp,
          bool is_visible,
          int argc,
          const char** argv,
          const char* initial_deep_link);

  std::unique_ptr<AppLifecycleRunner> runner_;

#if BUILDFLAG(IS_STARBOARD)
  // Ozone-specific bridge that converts Starboard events to Chromium events.
  // Non-Starboard platforms (like Android) handle these events natively.
  std::unique_ptr<ui::PlatformEventSourceStarboard> platform_event_source_;
#endif
};

}  // namespace cobalt

#endif  // COBALT_APP_APP_LIFECYCLE_DELEGATE_H_
