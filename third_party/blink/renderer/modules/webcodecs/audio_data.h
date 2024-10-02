// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_

#include "media/base/audio_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_sample_format.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class AudioDataInit;
class AudioDataCopyToOptions;
class ExceptionState;

class MODULES_EXPORT AudioData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioData* Create(AudioDataInit*, ExceptionState&);

  // Internal constructor for creating from media::AudioDecoder output.
  explicit AudioData(scoped_refptr<media::AudioBuffer>);

  // audio_data.idl implementation.
  explicit AudioData(AudioDataInit*, ExceptionState&);

  ~AudioData() final;

  // Creates a clone of |this|, taking on a new reference on |data_|. The cloned
  // frame will not be closed when |this| is, and its lifetime should be
  // independently managed.
  AudioData* clone(ExceptionState&);

  void close();

  absl::optional<V8AudioSampleFormat> format() const;
  float sampleRate() const;
  uint32_t numberOfFrames() const;
  uint32_t numberOfChannels() const;
  uint64_t duration() const;
  int64_t timestamp() const;

  uint32_t allocationSize(AudioDataCopyToOptions*, ExceptionState&);
  void copyTo(const AllowSharedBufferSource* destination,
              AudioDataCopyToOptions*,
              ExceptionState&);

  scoped_refptr<media::AudioBuffer> data() const { return data_; }

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 private:
  scoped_refptr<media::AudioBuffer> data_;

  absl::optional<V8AudioSampleFormat> format_;

  // Temporary space for converting to float32.
  std::unique_ptr<media::AudioBus> temp_bus_;

  int64_t timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_
