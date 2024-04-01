// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/media_gpu_channel.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/mojo/mojom/gpu_accelerated_video_decoder.mojom.h"

namespace media {

namespace {

class DecoderProviderImpl : public mojom::GpuAcceleratedVideoDecoderProvider,
                            public gpu::CommandBufferStub::DestructionObserver {
 public:
  DecoderProviderImpl(gpu::CommandBufferStub* stub,
                      const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
      : stub_(stub), overlay_factory_cb_(overlay_factory_cb) {}
  DecoderProviderImpl(const DecoderProviderImpl&) = delete;
  DecoderProviderImpl& operator=(const DecoderProviderImpl&) = delete;
  ~DecoderProviderImpl() override = default;

  // mojom::GpuAcceleratedVideoDecoderProvider:
  void CreateAcceleratedVideoDecoder(
      const VideoDecodeAccelerator::Config& config,
      mojo::PendingAssociatedReceiver<mojom::GpuAcceleratedVideoDecoder>
          receiver,
      mojo::PendingAssociatedRemote<mojom::GpuAcceleratedVideoDecoderClient>
          client,
      CreateAcceleratedVideoDecoderCallback callback) override {
    TRACE_EVENT0("gpu", "DecoderProviderImpl::CreateAcceleratedVideoDecoder");
    // Only allow stubs that have a ContextGroup, that is, the GLES2 ones. Later
    // code assumes the ContextGroup is valid.
    if (!stub_ || !stub_->decoder_context()->GetContextGroup()) {
      std::move(callback).Run(false);
      return;
    }

    // Note that `decoder` is a self-deleting object.
    GpuVideoDecodeAccelerator* decoder = new GpuVideoDecodeAccelerator(
        stub_, stub_->channel()->io_task_runner(), overlay_factory_cb_);
    std::move(callback).Run(
        decoder->Initialize(config, std::move(receiver), std::move(client)));
  }

 private:
  // gpu::CommandBufferStub::DestructionObserver:
  void OnWillDestroyStub(bool have_context) override { stub_ = nullptr; }

  gpu::CommandBufferStub* stub_;
  const AndroidOverlayMojoFactoryCB overlay_factory_cb_;
};

}  // namespace

MediaGpuChannel::MediaGpuChannel(
    gpu::GpuChannel* channel,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
    : channel_(channel), overlay_factory_cb_(overlay_factory_cb) {
  channel_->set_command_buffer_media_binder(
      base::BindRepeating(&MediaGpuChannel::BindCommandBufferMediaReceiver,
                          base::Unretained(this)));
}

MediaGpuChannel::~MediaGpuChannel() = default;

void MediaGpuChannel::BindCommandBufferMediaReceiver(
    gpu::CommandBufferStub* stub,
    mojo::GenericPendingAssociatedReceiver receiver) {
  if (auto r = receiver.As<mojom::GpuAcceleratedVideoDecoderProvider>()) {
    IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_binding;
    accelerated_video_decoder_providers_.Add(
        std::make_unique<DecoderProviderImpl>(stub, overlay_factory_cb_),
        std::move(r), stub->task_runner());
  }
}

}  // namespace media
