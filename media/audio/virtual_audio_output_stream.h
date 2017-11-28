// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_VIRTUAL_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_VIRTUAL_AUDIO_OUTPUT_STREAM_H_

#include "base/message_loop_proxy.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"

namespace media {

class AudioManagerBase;
class VirtualAudioInputStream;

// VirtualAudioOutputStream attaches to a VirtualAudioInputStream when Start()
// is called and is used as an audio source. VirtualAudioOutputStream also
// implements an interface so it can be used as an input to AudioConverter so
// that we can get audio frames that match the AudioParameters that
// VirtualAudioInputStream expects.
class MEDIA_EXPORT VirtualAudioOutputStream
    : public AudioOutputStream,
      public AudioConverter::InputCallback {
 public:
  static VirtualAudioOutputStream* MakeStream(
      AudioManagerBase* manager,
      const AudioParameters& params,
      base::MessageLoopProxy* message_loop,
      VirtualAudioInputStream* target);

  virtual ~VirtualAudioOutputStream();

  // AudioOutputStream:
  virtual bool Open() override;
  virtual void Start(AudioSourceCallback* callback) override;
  virtual void Stop() override;
  virtual void SetVolume(double volume) override;
  virtual void GetVolume(double* volume) override;
  virtual void Close() override;

 protected:
  VirtualAudioOutputStream(AudioManagerBase* manager,
                           const AudioParameters& params,
                           base::MessageLoopProxy* message_loop,
                           VirtualAudioInputStream* target);

 private:
  // AudioConverter::InputCallback:
  virtual double ProvideInput(AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) override;

  AudioManagerBase* audio_manager_;
  base::MessageLoopProxy* message_loop_;
  AudioSourceCallback* callback_;
  AudioParameters params_;

  // Pointer to the VirtualAudioInputStream to attach to when Start() is called.
  // This pointer should always be valid because VirtualAudioInputStream should
  // outlive this class.
  VirtualAudioInputStream* target_input_stream_;
  double volume_;
  bool attached_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_VIRTUAL_AUDIO_OUTPUT_STREAM_H_
