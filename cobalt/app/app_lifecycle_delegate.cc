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

#include "cobalt/app/app_lifecycle_delegate.h"

#include <unistd.h>

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include <init_musl.h>
#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/browser/loader_app_metrics.h"
#endif
#endif

namespace cobalt {

AppLifecycleDelegate::Callbacks::Callbacks() = default;
AppLifecycleDelegate::Callbacks::~Callbacks() = default;
AppLifecycleDelegate::Callbacks::Callbacks(const Callbacks&) = default;
AppLifecycleDelegate::Callbacks& AppLifecycleDelegate::Callbacks::operator=(
    const Callbacks&) = default;

AppLifecycleDelegate::AppLifecycleDelegate(Callbacks callbacks,
                                           bool skip_init_for_testing)
    : callbacks_(std::move(callbacks)),
      skip_init_for_testing_(skip_init_for_testing) {}

AppLifecycleDelegate::~AppLifecycleDelegate() = default;

void AppLifecycleDelegate::HandleEvent(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypePreload:
    case kSbEventTypeStart:
      OnStart(event);
      break;
    case kSbEventTypeStop:
      OnStop(event);
      break;
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
      if (platform_event_source_) {
        platform_event_source_->HandleFocusEvent(event);
      }
      break;
    case kSbEventTypeConceal:
      break;
    case kSbEventTypeReveal:
      if (callbacks_.on_reveal) {
        callbacks_.on_reveal.Run();
      }
      break;
    case kSbEventTypeFreeze:
    case kSbEventTypeUnfreeze:
      break;
    case kSbEventTypeInput:
      if (platform_event_source_) {
        platform_event_source_->HandleEvent(event);
      }
      break;
    case kSbEventTypeLink: {
      auto link = static_cast<const char*>(event->data);
      if (link && callbacks_.on_deep_link) {
        callbacks_.on_deep_link.Run(link);
      }
      break;
    }
    case kSbEventTypeLowMemory: {
      base::MemoryPressureListener::NotifyMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

      // Chromium internally calls Reclaim/ReclaimNormal at regular interval
      // to claim free memory. Using ReclaimAll is more aggressive.
      ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();

      if (event->data) {
        auto mem_cb = reinterpret_cast<SbEventCallback>(event->data);
        mem_cb(nullptr);
      }
      break;
    }
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged: {
      if (event->data && callbacks_.on_text_to_speech_state_changed) {
        auto* enabled = static_cast<const bool*>(event->data);
        callbacks_.on_text_to_speech_state_changed.Run(*enabled);
      }
      break;
    }
    case kSbEventTypeScheduled:
    case kSbEventTypeWindowSizeChanged:
      if (platform_event_source_) {
        platform_event_source_->HandleWindowSizeChangedEvent(event);
      }
      break;
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
    case kSbEventDateTimeConfigurationChanged:
      if (callbacks_.on_time_zone_change) {
        callbacks_.on_time_zone_change.Run();
      }
      break;
  }
}

void AppLifecycleDelegate::OnStart(const SbEvent* event) {
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  init_musl();
#endif
  SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
  if (!skip_init_for_testing_) {
    exit_manager_ = std::make_unique<base::AtExitManager>();
  }
  platform_event_source_ = std::make_unique<ui::PlatformEventSourceStarboard>();

  if (callbacks_.on_start) {
    callbacks_.on_start.Run(
        event->type == kSbEventTypeStart, data->argument_count,
        const_cast<const char**>(data->argument_values), data->link);
  }

#if BUILDFLAG(USE_EVERGREEN)
  // Log Loader App Metrics.
  cobalt::browser::RecordLoaderAppMetrics();
#endif
}

void AppLifecycleDelegate::OnStop(const SbEvent* event) {
  if (callbacks_.on_stop) {
    callbacks_.on_stop.Run();
  }

  platform_event_source_.reset();
  exit_manager_.reset();
}

}  // namespace cobalt
