// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gpu {
struct GpuPreferences;
}  // namespace gpu

namespace media {

// This class implements the interface mojom::VideoEncodeAccelerator.
class MEDIA_MOJO_EXPORT MojoVideoEncodeAcceleratorService
    : public mojom::VideoEncodeAccelerator,
      public VideoEncodeAccelerator::Client {
 public:
  // Create and initialize a VEA. Returns nullptr if either part fails.
  using CreateAndInitializeVideoEncodeAcceleratorCallback =
      base::OnceCallback<std::unique_ptr<::media::VideoEncodeAccelerator>(
          const ::media::VideoEncodeAccelerator::Config& config,
          Client* client,
          const gpu::GpuPreferences& gpu_preferences,
          const gpu::GpuDriverBugWorkarounds& gpu_workarounds)>;

  static void Create(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver,
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

  MojoVideoEncodeAcceleratorService(
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

  MojoVideoEncodeAcceleratorService(const MojoVideoEncodeAcceleratorService&) =
      delete;
  MojoVideoEncodeAcceleratorService& operator=(
      const MojoVideoEncodeAcceleratorService&) = delete;

  ~MojoVideoEncodeAcceleratorService() override;

  // mojom::VideoEncodeAccelerator impl.
  void Initialize(
      const media::VideoEncodeAccelerator::Config& config,
      mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> client,
      InitializeCallback callback) override;
  void Encode(const scoped_refptr<VideoFrame>& frame,
              bool force_keyframe,
              EncodeCallback callback) override;
  void UseOutputBitstreamBuffer(int32_t bitstream_buffer_id,
                                mojo::ScopedSharedBufferHandle buffer) override;
  void RequestEncodingParametersChangeWithBitrate(
      const media::Bitrate& bitrate_allocation,
      uint32_t framerate) override;
  void RequestEncodingParametersChangeWithLayers(
      const media::VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate) override;
  void IsFlushSupported(IsFlushSupportedCallback callback) override;
  void Flush(FlushCallback callback) override;

 private:
  friend class MojoVideoEncodeAcceleratorIntegrationTest;
  friend class MojoVideoEncodeAcceleratorServiceTest;

  // VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(::media::VideoEncodeAccelerator::Error error) override;
  void NotifyEncoderInfoChange(const ::media::VideoEncoderInfo& info) override;

  CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback_;
  const gpu::GpuPreferences& gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;

  // Owned pointer to the underlying VideoEncodeAccelerator.
  std::unique_ptr<::media::VideoEncodeAccelerator> encoder_;
  mojo::Remote<mojom::VideoEncodeAcceleratorClient> vea_client_;

  // Cache of parameters for sanity verification.
  size_t output_buffer_size_;
  gfx::Size input_coded_size_;

  // Note that this class is already thread hostile when bound.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MojoVideoEncodeAcceleratorService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_SERVICE_H_
