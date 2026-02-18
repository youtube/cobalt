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

#include "base/at_exit.h"
#include "base/functional/callback.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "starboard/event.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

namespace cobalt {

class AppLifecycleDelegate {
 public:
  struct Callbacks {
    // Called when the application starts (Start or Preload).
    // |is_visible| is true if it's a normal start, false if preloaded.
    base::RepeatingCallback<
        void(bool is_visible, int argc, const char** argv, const char* link)>
        on_start;

    // Called when the application is revealed to the foreground.
    base::RepeatingClosure on_reveal;

    // Called when the application is stopped.
    base::RepeatingClosure on_stop;

    base::RepeatingCallback<void(const std::string&)> on_deep_link;
    base::RepeatingClosure on_time_zone_change;
    base::RepeatingCallback<void(bool)> on_text_to_speech_state_changed;

    Callbacks();
    ~Callbacks();
    Callbacks(const Callbacks&);
    Callbacks& operator=(const Callbacks&);
  };

  explicit AppLifecycleDelegate(Callbacks callbacks,
                                bool skip_init_for_testing = false);
  ~AppLifecycleDelegate();

  void HandleEvent(const SbEvent* event);

 private:
  void OnStart(const SbEvent* event);
  void OnStop(const SbEvent* event);

  Callbacks callbacks_;
  bool skip_init_for_testing_;
  std::unique_ptr<base::AtExitManager> exit_manager_;
  std::unique_ptr<cobalt::CobaltMainDelegate> content_main_delegate_;
  std::unique_ptr<ui::PlatformEventSourceStarboard> platform_event_source_;
};

}  // namespace cobalt

#endif  // COBALT_APP_APP_LIFECYCLE_DELEGATE_H_
