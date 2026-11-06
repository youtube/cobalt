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

#ifndef COBALT_SHELL_BROWSER_SPLASH_SCREEN_METRICS_H_
#define COBALT_SHELL_BROWSER_SPLASH_SCREEN_METRICS_H_

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "base/time/time.h"

namespace content {

// Collects the timing of the startup sequence of the splash screen and the
// main WebContents (WebApp), and records it as UMA once per launch:
// - With a splash screen, the timing is recorded when switching to the main
//   WebContents.
// - Without one, the main WebContents' timeline is recorded once it has both
//   painted and loaded.
// - kMainReady is recorded when it happens, or when switching to the main
//   WebContents if it happens before that, once the mode (Image or Video) is
//   final.
// Launches that start hidden, or are concealed before the metrics are
// recorded, aren't recorded.
//
// Lifetime and ownership: owned by the first Shell created in the process (see
// MaybeCreateForLaunch()), and destroyed with it.
//
// Threading: thread-affine; it and its static methods must only be used on the
// browser UI thread (the main thread).
class SplashScreenMetrics {
 public:
  enum class Event {
    // Shell started loading the splash screen.
    kSplashStart,
    // The splash screen finished loading. The minimum splash screen duration
    // starts when its content shows instead, within
    // kSplashScreenContentWaitMaxDelay (2s by default) of this: for an image,
    // at kSplashFirstContentfulPaint, and for a video, at kSplashVideoStarted.
    kSplashLoaded,
    // The splash screen's first visually non-empty paint. That may be its
    // background alone, as Blink counts a page as visually non-empty once it
    // has finished parsing.
    kSplashFirstPaint,
    // The splash screen's first contentful paint. If its page shows its image
    // (see OnSplashScreenShowsImage()), this is when the image is shown.
    // Otherwise, it's the paint of its video element, which counts as
    // contentful before the video's first frame.
    kSplashFirstContentfulPaint,
    // The splash screen video started playing.
    kSplashVideoStarted,
    // The splash screen video reached its end.
    kSplashVideoEnded,
    // The splash screen closed itself (window.close()).
    kSplashClosed,
    // The main WebContents started its first navigation.
    kMainNavigationStart,
    // The main WebContents committed its first navigation.
    kMainCommit,
    // The main WebContents' first visually non-empty paint.
    kMainFirstPaint,
    // The main WebContents finished loading.
    kMainLoaded,
    // The main WebContents called h5vcc.system.hideSplashScreen().
    kMainReady,
    kMaxValue = kMainReady,
  };

  // What caused the switch from the splash screen to the main WebContents.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Keep in sync with
  // CobaltSplashScreenSwitchTrigger in
  // tools/metrics/histograms/metadata/cobalt/enums.xml.
  enum class SwitchTrigger {
    // The minimum splash screen duration elapsed after the main WebContents
    // had loaded.
    kTimer = 0,
    // The splash screen closed itself after the main WebContents had loaded.
    kSplashClosed = 1,
    // The main WebContents finished loading after the splash screen was done,
    // i.e., the minimum duration had elapsed or the splash screen had closed
    // itself.
    kMainLoaded = 2,
    kMaxValue = kMainLoaded,
  };

  // What the minimum splash screen duration waits for before it starts.
  enum class SplashContent {
    // Its image to be painted.
    kImage,
    // Its video to start playing.
    kVideo,
  };

  // How the minimum splash screen duration's wait for the splash screen
  // content ended. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused. Keep in sync with
  // CobaltSplashScreenContentWaitResult in
  // tools/metrics/histograms/metadata/cobalt/enums.xml.
  enum class ContentWaitResult {
    // The content showed within kSplashScreenContentWaitMaxDelay of the page
    // of the splash screen loading.
    kContentShown = 0,
    // It didn't, so the minimum duration started anyway.
    kTimedOut = 1,
    // Cobalt switched to the main WebContents before either, e.g., because the
    // splash screen closed itself when it couldn't show its image.
    kSwitchedBeforeContent = 2,
    kMaxValue = kSwitchedBeforeContent,
  };

  // Sets the start of the launch, which the timeline histograms are measured
  // from. Only the first call has an effect.
  static void SetAppStartTime(base::TimeTicks start_time);

  // Returns a new instance if no launch has been tracked yet in this process,
  // or nullptr otherwise, so that only the first window is measured.
  static std::unique_ptr<SplashScreenMetrics> MaybeCreateForLaunch(
      bool has_splash_screen,
      bool is_visible);

  static void ResetForTesting();

  SplashScreenMetrics(const SplashScreenMetrics&) = delete;
  SplashScreenMetrics& operator=(const SplashScreenMetrics&) = delete;

  ~SplashScreenMetrics();

  // Marks that `event` happened now. Only the first occurrence counts.
  void MarkEvent(Event event);

  // Marks that the page of the splash screen shows its image rather than its
  // video, so that its first contentful paint is when its image shows: for the
  // image splash screen, or the video splash screen if it can't play its video.
  // Otherwise, that paint is its video element's, and doesn't count as the
  // splash screen content being shown.
  void OnSplashScreenShowsImage();

  // Marks how the minimum splash screen duration's wait for `content` ended.
  // Recorded when switching to the main WebContents, so the wait must end by
  // then. Only the first call counts.
  void OnContentWaitEnded(SplashContent content, ContentWaitResult result);

  // Drops the metrics of this launch, e.g., because the app was concealed.
  void Invalidate();

  // Records the metrics of a launch with a splash screen. Must be called when
  // switching from the splash screen to the main WebContents.
  void OnSwitchedToMain(SwitchTrigger trigger);

 private:
  // What the minimum splash screen duration waited for, and how the wait
  // ended.
  struct ContentWait {
    SplashContent content;
    ContentWaitResult result;
  };

  SplashScreenMetrics(bool has_splash_screen, bool is_visible);

  base::TimeTicks GetTime(Event event) const;

  // Returns the mode of the launch: Image, Video or NoSplash.
  std::string_view GetMode() const;

  // Records the metrics of a launch without a splash screen, once the main
  // WebContents has both painted and loaded.
  void MaybeRecordWithoutSplashScreen();

  // Records the timeline of the main WebContents shared by all modes.
  void RecordMainTimeline(std::string_view mode) const;

  // Records kMainReady once it has happened and the mode is final, i.e., with a
  // splash screen, once switched to the main WebContents.
  void MaybeRecordMainReady() const;

  const bool has_splash_screen_;
  bool is_valid_;
  bool has_recorded_ = false;
  // Whether the page of the splash screen shows its image.
  bool shows_image_ = false;
  // Set once the wait for the splash screen content ended, if there was one.
  std::optional<ContentWait> content_wait_;
  std::array<base::TimeTicks, static_cast<size_t>(Event::kMaxValue) + 1>
      event_times_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SCREEN_METRICS_H_
