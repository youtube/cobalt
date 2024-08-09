// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "third_party/chromium/media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "third_party/chromium/media/mojo/services/media_mojo_export.h"
#include "third_party/chromium/media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace gpu {
struct GpuPreferences;
class GpuDriverBugWorkarounds;
}  // namespace gpu

namespace media_m96 {

// This class implements the interface mojom::VideoEncodeAcceleratorProvider,
// holds on to the necessary objects to create mojom::VideoEncodeAccelerators.
class MEDIA_MOJO_EXPORT MojoVideoEncodeAcceleratorProvider
    : public mojom::VideoEncodeAcceleratorProvider {
 public:
  using CreateAndInitializeVideoEncodeAcceleratorCallback =
      base::RepeatingCallback<std::unique_ptr<::media_m96::VideoEncodeAccelerator>(
          const ::media_m96::VideoEncodeAccelerator::Config& config,
          VideoEncodeAccelerator::Client* client,
          const gpu::GpuPreferences& gpu_preferences,
          const gpu::GpuDriverBugWorkarounds& gpu_workarounds)>;

  static void Create(
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver,
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

  MojoVideoEncodeAcceleratorProvider(
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

  MojoVideoEncodeAcceleratorProvider(
      const MojoVideoEncodeAcceleratorProvider&) = delete;
  MojoVideoEncodeAcceleratorProvider& operator=(
      const MojoVideoEncodeAcceleratorProvider&) = delete;

  ~MojoVideoEncodeAcceleratorProvider() override;

  // mojom::VideoEncodeAcceleratorProvider impl.
  void CreateVideoEncodeAccelerator(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) override;
  void GetVideoEncodeAcceleratorSupportedProfiles(
      GetVideoEncodeAcceleratorSupportedProfilesCallback callback) override;

 private:
  const CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback_;
  const gpu::GpuPreferences& gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_
