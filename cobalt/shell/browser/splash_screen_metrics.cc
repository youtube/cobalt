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

#include "cobalt/shell/browser/splash_screen_metrics.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace content {

namespace {

constexpr char kTimelinePrefix[] = "Cobalt.Startup.Timeline.";
constexpr char kSplashScreenPrefix[] = "Cobalt.SplashScreen.";

constexpr char kImageMode[] = "Image";
constexpr char kVideoMode[] = "Video";
constexpr char kNoSplashMode[] = "NoSplash";

// The start of the launch. Only accessed on the main thread.
base::TimeTicks g_app_start_time;
// Whether a launch has already been tracked in this process.
bool g_has_tracked_launch = false;

const char* SwitchTriggerToString(SplashScreenMetrics::SwitchTrigger trigger) {
  switch (trigger) {
    case SplashScreenMetrics::SwitchTrigger::kTimer:
      return "Timer";
    case SplashScreenMetrics::SwitchTrigger::kSplashClosed:
      return "SplashClosed";
    case SplashScreenMetrics::SwitchTrigger::kMainLoaded:
      return "MainLoaded";
  }
  NOTREACHED();
}

void RecordTime(const std::string& name,
                base::TimeDelta sample,
                base::TimeDelta min,
                base::TimeDelta max) {
  VLOG(1) << "NativeSplash: " << name << ": " << sample.InMilliseconds()
          << "ms";
  base::UmaHistogramCustomTimes(name, sample, min, max, /*buckets=*/100);
}

// Records the time from the start of the launch to `time`, if both are known.
void RecordTimeline(std::string_view event,
                    base::TimeTicks time,
                    std::string_view mode) {
  if (g_app_start_time.is_null() || time.is_null()) {
    return;
  }
  RecordTime(base::StrCat({kTimelinePrefix, event, ".", mode}),
             time - g_app_start_time, base::Milliseconds(10),
             base::Seconds(60));
}

// Records the time from `from` to `to`, if both are known.
void RecordInterval(std::string_view interval,
                    base::TimeTicks from,
                    base::TimeTicks to,
                    std::string_view mode) {
  if (from.is_null() || to.is_null()) {
    return;
  }
  RecordTime(base::StrCat({kSplashScreenPrefix, "Time.", interval, ".", mode}),
             std::max(to - from, base::TimeDelta()), base::Milliseconds(1),
             base::Seconds(10));
}

}  // namespace

// static
void SplashScreenMetrics::SetAppStartTime(base::TimeTicks start_time) {
  if (g_app_start_time.is_null()) {
    g_app_start_time = start_time;
  }
}

// static
std::unique_ptr<SplashScreenMetrics> SplashScreenMetrics::MaybeCreateForLaunch(
    bool has_splash_screen,
    bool is_visible) {
  if (g_has_tracked_launch) {
    return nullptr;
  }
  g_has_tracked_launch = true;
  return base::WrapUnique(
      new SplashScreenMetrics(has_splash_screen, is_visible));
}

// static
void SplashScreenMetrics::ResetForTesting() {
  g_app_start_time = base::TimeTicks();
  g_has_tracked_launch = false;
}

SplashScreenMetrics::SplashScreenMetrics(bool has_splash_screen,
                                         bool is_visible)
    : has_splash_screen_(has_splash_screen), is_valid_(is_visible) {}

SplashScreenMetrics::~SplashScreenMetrics() = default;

void SplashScreenMetrics::MarkEvent(Event event) {
  base::TimeTicks& time = event_times_[static_cast<size_t>(event)];
  if (!time.is_null()) {
    return;
  }
  time = base::TimeTicks::Now();
  if (event == Event::kMainReady) {
    RecordMainReady();
  } else if (!has_splash_screen_ &&
             (event == Event::kMainFirstPaint || event == Event::kMainLoaded)) {
    MaybeRecordWithoutSplashScreen();
  }
}

void SplashScreenMetrics::Invalidate() {
  if (is_valid_ && !has_recorded_) {
    VLOG(1) << "NativeSplash: Not recording startup metrics for this launch.";
  }
  is_valid_ = false;
}

void SplashScreenMetrics::OnSwitchedToMain(SwitchTrigger trigger) {
  if (!has_splash_screen_ || !is_valid_ || has_recorded_) {
    return;
  }
  has_recorded_ = true;
  const base::TimeTicks switch_time = base::TimeTicks::Now();

  const std::string_view mode = GetMode();
  const bool is_video = mode == kVideoMode;
  const base::TimeTicks content_shown =
      GetTime(is_video ? Event::kSplashVideoStarted : Event::kSplashFirstPaint);
  const base::TimeTicks content_ended =
      GetTime(is_video ? Event::kSplashVideoEnded : Event::kSplashClosed);

  RecordTimeline("SplashStart", GetTime(Event::kSplashStart), mode);
  RecordTimeline("SplashLoaded", GetTime(Event::kSplashLoaded), mode);
  RecordTimeline("SplashFirstPaint", GetTime(Event::kSplashFirstPaint), mode);
  RecordTimeline("SplashContentShown", content_shown, mode);
  RecordTimeline("SplashContentEnded", content_ended, mode);
  RecordMainTimeline(mode);
  // With a splash screen, the main WebContents is visible from the switch.
  RecordTimeline("MainVisible", switch_time, mode);

  RecordInterval("PageLoad", GetTime(Event::kSplashStart),
                 GetTime(Event::kSplashLoaded), mode);
  RecordInterval("LoadedToContentShown", GetTime(Event::kSplashLoaded),
                 content_shown, mode);
  RecordInterval("ContentShownToSwitch", content_shown, switch_time, mode);
  RecordInterval("MainLoadedToSwitch", GetTime(Event::kMainLoaded), switch_time,
                 mode);

  const bool content_ended_before_switch = !content_ended.is_null();
  const std::string content_ended_name =
      base::StrCat({kSplashScreenPrefix, "ContentEndedBeforeSwitch.", mode});
  VLOG(1) << "NativeSplash: " << content_ended_name << ": "
          << (content_ended_before_switch ? "true" : "false");
  base::UmaHistogramBoolean(content_ended_name, content_ended_before_switch);

  const std::string trigger_name =
      base::StrCat({kSplashScreenPrefix, "SwitchTrigger.", mode});
  VLOG(1) << "NativeSplash: " << trigger_name << ": "
          << SwitchTriggerToString(trigger);
  base::UmaHistogramEnumeration(trigger_name, trigger);
}

base::TimeTicks SplashScreenMetrics::GetTime(Event event) const {
  return event_times_[static_cast<size_t>(event)];
}

std::string_view SplashScreenMetrics::GetMode() const {
  if (!has_splash_screen_) {
    return kNoSplashMode;
  }
  // The video splash screen falls back to an image, e.g., if the device
  // doesn't support VP9, so the mode is the one actually shown.
  return GetTime(Event::kSplashVideoStarted).is_null() ? kImageMode
                                                       : kVideoMode;
}

void SplashScreenMetrics::RecordMainReady() const {
  if (!is_valid_) {
    return;
  }
  RecordTimeline("MainReady", GetTime(Event::kMainReady), GetMode());
}

void SplashScreenMetrics::MaybeRecordWithoutSplashScreen() {
  const base::TimeTicks first_paint = GetTime(Event::kMainFirstPaint);
  if (!is_valid_ || has_recorded_ || first_paint.is_null() ||
      GetTime(Event::kMainLoaded).is_null()) {
    return;
  }
  has_recorded_ = true;
  RecordMainTimeline(kNoSplashMode);
  // Without a splash screen, the main WebContents is visible from its first
  // paint.
  RecordTimeline("MainVisible", first_paint, kNoSplashMode);
}

void SplashScreenMetrics::RecordMainTimeline(std::string_view mode) const {
  RecordTimeline("MainNavigationStart", GetTime(Event::kMainNavigationStart),
                 mode);
  RecordTimeline("MainCommit", GetTime(Event::kMainCommit), mode);
  RecordTimeline("MainLoaded", GetTime(Event::kMainLoaded), mode);
}

}  // namespace content
