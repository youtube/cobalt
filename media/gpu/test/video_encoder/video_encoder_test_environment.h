// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_

#include <limits>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {

class Bitrate;

namespace test {
class Video;

// Test environment for video encoder tests. Performs setup and teardown once
// for the entire test run.
class VideoEncoderTestEnvironment : public VideoTestEnvironment {
 public:
  // VideoEncoderTest uses at most 60 frames in the given video file.
  // This limitation is required as a long video stream might not fit in
  // a device's memory or the number of allocatable handles in the system.
  // TODO(hiroh): Streams frames from disk so we can avoid this limitation when
  // encoding long video streams.
  static constexpr size_t kMaxReadFrames = 60;

  static VideoEncoderTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      bool enable_bitstream_validator,
      const base::FilePath& output_folder,
      const std::string& codec,
      size_t num_temporal_layers,
      size_t num_spatial_layers,
      bool save_output_bitstream,
      absl::optional<uint32_t> output_bitrate,
      Bitrate::Mode bitrate_mode,
      bool reverse,
      const FrameOutputConfig& frame_output_config = FrameOutputConfig(),
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {});

  ~VideoEncoderTestEnvironment() override;

  // Get the video the tests will be ran on.
  media::test::Video* Video() const;
  // Generate the nv12 video from |video_| the test will be ran on.
  media::test::Video* GenerateNV12Video();
  // Whether bitstream validation is enabled.
  bool IsBitstreamValidatorEnabled() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;
  // Get the output codec profile.
  VideoCodecProfile Profile() const;
  // Get the spatial layers config for SVC. Return empty vector in non SVC mode.
  const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
  SpatialLayers() const;
  // Get the target bitrate (bits/second).
  const VideoBitrateAllocation& BitrateAllocation() const;
  // Whether the encoded bitstream is saved to disk.
  bool SaveOutputBitstream() const;
  // True if the video should play backwards at reaching the end of video.
  // Otherwise the video loops. See the comment in AlignedDataHelper for detail.
  bool Reverse() const;
  absl::optional<base::FilePath> OutputBitstreamFilePath() const;
  // Gets the frame output configuration.
  const FrameOutputConfig& ImageOutputConfig() const;

  // Get the GpuMemoryBufferFactory for doing buffer allocations. This needs to
  // survive as long as the process is alive just like in production which is
  // why it's in here as there are threads that won't immediately die when an
  // individual test is completed.
  gpu::GpuMemoryBufferFactory* GetGpuMemoryBufferFactory() const;

  // Returns whether kepler will be used in the test.
  bool IsKeplerUsed() const;

 private:
  // TODO(crbug.com/1335251): merge |use_vbr| and |bitrate| into a single
  // Bitrate-typed field.
  VideoEncoderTestEnvironment(
      std::unique_ptr<media::test::Video> video,
      bool enable_bitstream_validator,
      const base::FilePath& output_folder,
      VideoCodecProfile profile,
      size_t num_temporal_layers,
      size_t num_spatial_layers,
      const media::Bitrate& bitrate,
      bool save_output_bitstream,
      bool reverse,
      const FrameOutputConfig& frame_output_config,
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);

  // Video file to be used for testing.
  const std::unique_ptr<media::test::Video> video_;
  // NV12 video file to be used for testing.
  std::unique_ptr<media::test::Video> nv12_video_;
  // Whether bitstream validation should be enabled while testing.
  const bool enable_bitstream_validator_;
  // Output folder to be used to store test artifacts (e.g. perf metrics).
  const base::FilePath output_folder_;
  // VideoCodecProfile to be produced by VideoEncoder.
  const VideoCodecProfile profile_;
  // The number of temporal layers of the stream to be produced by VideoEncoder.
  // This is only for vp9 stream.
  const size_t num_temporal_layers_;
  // The number of spatial layers of the stream to be produced by VideoEncoder.
  // This is only for vp9 stream.
  const size_t num_spatial_layers_;
  // Targeted bitrate (bits/second) of the stream produced by VideoEncoder.
  const VideoBitrateAllocation bitrate_;
  // The spatial layers of the stream which aligned with |num_spatial_layers_|
  // and |num_temporal_layers_|. This is only for vp9 stream.
  std::vector<VideoEncodeAccelerator::Config::SpatialLayer> spatial_layers_;
  // Whether the bitstream produced by VideoEncoder is saved to disk.
  const bool save_output_bitstream_;
  // True if the video should play backwards at reaching the end of video.
  // Otherwise the video loops. See the comment in AlignedDataHelper for detail.
  const bool reverse_;
  // The configuration about saving decoded images of bitstream encoded by
  // VideoEncoder.
  // The configuration used when saving the decoded images of bitstream encoded
  // by VideoEncoder to disk.
  const FrameOutputConfig frame_output_config_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
