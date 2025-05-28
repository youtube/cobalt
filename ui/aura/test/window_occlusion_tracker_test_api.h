// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_OCCLUSION_TRACKER_TEST_API_H_
#define UI_AURA_TEST_WINDOW_OCCLUSION_TRACKER_TEST_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace aura {

class WindowOcclusionTracker;

namespace test {

class WindowOcclusionTrackerTestApi {
 public:
  explicit WindowOcclusionTrackerTestApi(WindowOcclusionTracker* tracker);

  WindowOcclusionTrackerTestApi(const WindowOcclusionTrackerTestApi&) = delete;
  WindowOcclusionTrackerTestApi& operator=(
      const WindowOcclusionTrackerTestApi&) = delete;

  ~WindowOcclusionTrackerTestApi();

  // Creates a WindowOcclusionTracker for TestWindowTreeClientSetup to simulate
  // server side behavior. In most cases, tests should NOT call this and use the
  // instance in Env instead.
  static std::unique_ptr<WindowOcclusionTracker> Create();

  // Returns the number of times that occlusion was recomputed in this process.
  int GetNumTimesOcclusionRecomputed() const;

  void Pause();
  void Unpause();

  bool IsPaused() const;

 private:
  const raw_ptr<WindowOcclusionTracker> tracker_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_WINDOW_OCCLUSION_TRACKER_TEST_API_H_
