// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_H_

#include "base/unguessable_token.h"
#include "third_party/chromium/media/base/android_overlay_mojo_factory.h"
#include "third_party/chromium/media/mojo/mojom/gpu_accelerated_video_decoder.mojom.h"
#include "third_party/chromium/media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"

namespace gpu {
class CommandBufferStub;
class GpuChannel;
}

namespace media_m96 {

class MediaGpuChannel {
 public:
  MediaGpuChannel(gpu::GpuChannel* channel,
                  const AndroidOverlayMojoFactoryCB& overlay_factory_cb);
  MediaGpuChannel(const MediaGpuChannel&) = delete;
  MediaGpuChannel& operator=(const MediaGpuChannel&) = delete;
  ~MediaGpuChannel();

 private:
  void BindCommandBufferMediaReceiver(
      gpu::CommandBufferStub* stub,
      mojo::GenericPendingAssociatedReceiver receiver);

  gpu::GpuChannel* const channel_;
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;
  mojo::UniqueAssociatedReceiverSet<mojom::GpuAcceleratedVideoDecoderProvider>
      accelerated_video_decoder_providers_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_H_
