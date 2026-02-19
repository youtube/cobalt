// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <string.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "cobalt/app/app_lifecycle_delegate.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "cobalt/app/cobalt_switch_defaults_starboard.h"
#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "services/device/time_zone_monitor/time_zone_monitor_starboard.h"
#include "starboard/event.h"

namespace {

static cobalt::AppLifecycleDelegate* g_lifecycle_delegate = nullptr;
static std::unique_ptr<cobalt::CobaltMainDelegate> g_content_main_delegate;

content::ContentMainRunner* GetContentMainRunner() {
  static base::NoDestructor<std::unique_ptr<content::ContentMainRunner>> runner{
      content::ContentMainRunner::Create()};
  return runner->get();
}

int InitCobalt(bool is_visible,
               int argc,
               const char** argv,
               const char* initial_deep_link) {
  g_content_main_delegate = std::make_unique<cobalt::CobaltMainDelegate>(
      false /* is_content_browsertests */, is_visible);

  content::ContentMainParams params(g_content_main_delegate.get());

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  // Preprocess the raw command line arguments with the defaults expected by
  // Cobalt.
  cobalt::CommandLinePreprocessor init_cmd_line(argc, argv);
  const auto& init_argv = init_cmd_line.argv();

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  logging::SetMinLogLevel(logging::LOGGING_FATAL);
#endif

  std::stringstream ss;
  std::vector<const char*> args;
  for (const auto& arg : init_argv) {
    args.push_back(arg.c_str());
    ss << " " << arg;
  }
  LOG(INFO) << "Parsed command line string:" << ss.str();

  if (initial_deep_link) {
    auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
    manager->set_deep_link(initial_deep_link);
  }

  // This expression exists to ensure that we apply the argument overrides
  // only on the main process, not on spawned processes such as the zygote.
#if !BUILDFLAG(IS_ANDROID)
  if ((!strcmp(argv[0], "/proc/self/exe")) ||
      ((argc >= 2) && !strcmp(argv[1], "--type=zygote"))) {
    params.argc = argc;
    params.argv = argv;
  } else {
    params.argc = args.size();
    params.argv = args.data();
  }
#endif

  return RunContentProcess(std::move(params), GetContentMainRunner());
}

void OnStop() {
  content::Shell::Shutdown();

  if (g_content_main_delegate) {
    g_content_main_delegate->Shutdown();
  }

  GetContentMainRunner()->Shutdown();

  g_content_main_delegate.reset();
}

}  // namespace

void SbEventHandle(const SbEvent* event) {
  if (!g_lifecycle_delegate) {
    cobalt::AppLifecycleDelegate::Callbacks callbacks;
    callbacks.on_start = base::BindRepeating(
        [](bool is_visible, int argc, const char** argv, const char* link) {
          InitCobalt(is_visible, argc, argv, link);
        });
    callbacks.on_reveal = base::BindRepeating(&content::Shell::OnReveal);
    callbacks.on_stop = base::BindRepeating(&OnStop);
    callbacks.on_deep_link = base::BindRepeating([](const std::string& link) {
      auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
      manager->OnDeepLink(link.c_str());
    });
    callbacks.on_time_zone_change =
        base::BindRepeating(&device::NotifyTimeZoneChangeStarboard);
    callbacks.on_text_to_speech_state_changed =
        base::BindRepeating([](bool enabled) {
          cobalt::browser::H5vccAccessibilityManager::GetInstance()
              ->OnTextToSpeechStateChanged(enabled);
        });
    g_lifecycle_delegate =
        new cobalt::AppLifecycleDelegate(std::move(callbacks));
  }
  g_lifecycle_delegate->HandleEvent(event);
  if (event->type == kSbEventTypeStop) {
    delete g_lifecycle_delegate;
    g_lifecycle_delegate = nullptr;
  }
}

#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
