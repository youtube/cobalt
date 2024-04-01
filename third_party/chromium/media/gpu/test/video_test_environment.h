// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper classes for video accelerator unittests.

#ifndef MEDIA_GPU_TEST_VIDEO_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_TEST_ENVIRONMENT_H_

#include <memory>
#include <vector>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class TaskEnvironment;
}  // namespace test
}  // namespace base

namespace media {
namespace test {

// The frame output mode allows controlling which video frames are written to
// disk. Writing frames will greatly slow down the test and generate a lot of
// test artifacts, so be careful when configuring other modes than kNone in
// automated testing.
enum class FrameOutputMode {
  kNone,     // Don't output any frames.
  kCorrupt,  // Only output corrupt frames.
  kAll       // Output all frames.
};

// Frame output configuration.
struct FrameOutputConfig {
  // The frame output mode controls which frames will be output.
  FrameOutputMode output_mode = FrameOutputMode::kNone;
  // The maximum number of frames that will be output.
  uint64_t output_limit = std::numeric_limits<uint64_t>::max();
  // The format of frames that are output.
  VideoFrameFileWriter::OutputFormat output_format =
      VideoFrameFileWriter::OutputFormat::kPNG;
};

class VideoTestEnvironment : public ::testing::Environment {
 public:
  VideoTestEnvironment();
  // Features are overridden by given features in this environment.
  VideoTestEnvironment(const std::vector<base::Feature>& enabled_features,
                       const std::vector<base::Feature>& disabled_features);
  virtual ~VideoTestEnvironment();

  // ::testing::Environment implementation.
  // Tear down video test environment, called once for entire test run.
  void TearDown() override;

  // Get the name of the test output file path (testsuitename/testname).
  base::FilePath GetTestOutputFilePath() const;

 private:
  // An exit manager is required to run callbacks on shutdown.
  base::AtExitManager at_exit_manager;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  // Features to override default feature settings in this environment.
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_TEST_ENVIRONMENT_H_
