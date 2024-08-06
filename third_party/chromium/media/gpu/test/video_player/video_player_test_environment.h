// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_

#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "third_party/chromium/media/gpu/test/video_frame_file_writer.h"
#include "third_party/chromium/media/gpu/test/video_player/video_decoder_client.h"
#include "third_party/chromium/media/gpu/test/video_test_environment.h"

namespace media_m96 {
namespace test {

class Video;

// Test environment for video decode tests. Performs setup and teardown once for
// the entire test run.
class VideoPlayerTestEnvironment : public VideoTestEnvironment {
 public:
  enum class ValidatorType {
    kNone,
    kMD5,
    kSSIM,
  };

  static VideoPlayerTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      ValidatorType validator_type,
      const DecoderImplementation implementation,
      const base::FilePath& output_folder = base::FilePath(),
      const FrameOutputConfig& frame_output_config = FrameOutputConfig());
  ~VideoPlayerTestEnvironment() override;

  // Get the video the tests will be ran on.
  const media_m96::test::Video* Video() const;
  // Check whether frame validation is enabled.
  bool IsValidatorEnabled() const;
  // Get the validator type.
  ValidatorType GetValidatorType() const;
  // Return which implementation is used.
  DecoderImplementation GetDecoderImplementation() const;

  // Get the frame output mode.
  FrameOutputMode GetFrameOutputMode() const;
  // Get the file format used when outputting frames.
  VideoFrameFileWriter::OutputFormat GetFrameOutputFormat() const;
  // Get the maximum number of frames that will be output.
  uint64_t GetFrameOutputLimit() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;

  // Get the GpuMemoryBufferFactory for doing buffer allocations. This needs to
  // survive as long as the process is alive just like in production which is
  // why it's in here as there are threads that won't immediately die when an
  // individual test is completed.
  gpu::GpuMemoryBufferFactory* GetGpuMemoryBufferFactory() const;

 private:
  VideoPlayerTestEnvironment(std::unique_ptr<media_m96::test::Video> video,
                             ValidatorType validator_type,
                             const DecoderImplementation implementation,
                             const base::FilePath& output_folder,
                             const FrameOutputConfig& frame_output_config);

  const std::unique_ptr<media_m96::test::Video> video_;
  const ValidatorType validator_type_;
  const DecoderImplementation implementation_;

  const FrameOutputConfig frame_output_config_;
  const base::FilePath output_folder_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
};
}  // namespace test
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
