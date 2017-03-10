// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_DECODER_FACTORY_H_
#define COBALT_MEDIA_BASE_DECODER_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cobalt {
namespace media {

class AudioDecoder;
class GpuVideoAcceleratorFactories;
class VideoDecoder;

// A factory class for creating audio and video decoders.
class MEDIA_EXPORT DecoderFactory {
 public:
  DecoderFactory();
  virtual ~DecoderFactory();

  // Creates audio decoders and append them to the end of |audio_decoders|.
  // Decoders are single-threaded, each decoder should run on |task_runner|.
  virtual void CreateAudioDecoders(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ScopedVector<AudioDecoder>* audio_decoders);

  // Creates video decoders and append them to the end of |video_decoders|.
  // Decoders are single-threaded, each decoder should run on |task_runner|.
  virtual void CreateVideoDecoders(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      ScopedVector<VideoDecoder>* video_decoders);

 private:
  DISALLOW_COPY_AND_ASSIGN(DecoderFactory);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DECODER_FACTORY_H_
