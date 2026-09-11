/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_TEST_UTILS_H_
#define VIDEO_TIMING_SIMULATOR_TEST_UTILS_H_

#include <cstdint>
#include <memory>

#include "absl/functional/any_invocable.h"
#include "api/environment/environment.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/encoded_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc::video_timing_simulator {

// Frame ID matchers.
inline ::testing::Matcher<const EncodedFrame&> EncodedFrameWithId(
    int64_t expected_id) {
  return ::testing::Property(&EncodedFrame::Id, ::testing::Eq(expected_id));
}
inline ::testing::Matcher<std::unique_ptr<EncodedFrame>> EncodedFramePtrWithId(
    int64_t expected_id) {
  return ::testing::Pointee(EncodedFrameWithId(expected_id));
}

// Fixture for running objects under test on a simulated time task queue.
class SimulatedTimeTestFixture : public ::testing::Test {
 protected:
  SimulatedTimeTestFixture();
  ~SimulatedTimeTestFixture();

  // Post a task to the simulated time task queue and synchronize.
  void SendTask(absl::AnyInvocable<void() &&> task);

  // Environment.
  GlobalSimulatedTimeController time_controller_;
  const Environment env_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> queue_;
  TaskQueueBase* queue_ptr_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_TEST_UTILS_H_
