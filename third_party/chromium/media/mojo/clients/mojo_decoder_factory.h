// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/chromium/media/base/decoder_factory.h"

namespace media_m96 {

namespace mojom {
class InterfaceFactory;
}

class MojoDecoderFactory final : public DecoderFactory {
 public:
  explicit MojoDecoderFactory(
      media_m96::mojom::InterfaceFactory* interface_factory);

  MojoDecoderFactory(const MojoDecoderFactory&) = delete;
  MojoDecoderFactory& operator=(const MojoDecoderFactory&) = delete;

  ~MojoDecoderFactory() final;

  void CreateAudioDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) final;

  // TODO(crbug.com/1173503): Implement GetSupportedVideoDecoderConfigs.

  void CreateVideoDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) final;

 private:
  media_m96::mojom::InterfaceFactory* interface_factory_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_
