// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_input_stream.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/audio/virtual_audio_output_stream.h"

namespace media {

// LoopbackAudioConverter works similar to AudioConverter and converts input
// streams to different audio parameters. Then, the LoopbackAudioConverter can
// be used as an input to another AudioConverter. This allows us to
// use converted audio from AudioOutputStreams as input to an AudioConverter.
// For example, this allows converting multiple streams into a common format and
// using the converted audio as input to another AudioConverter (i.e. a mixer).
class LoopbackAudioConverter : public AudioConverter::InputCallback {
 public:
  LoopbackAudioConverter(const AudioParameters& input_params,
                         const AudioParameters& output_params)
      : audio_converter_(input_params, output_params, false) {}

  virtual ~LoopbackAudioConverter() {}

  void AddInput(AudioConverter::InputCallback* input) {
    audio_converter_.AddInput(input);
  }

  void RemoveInput(AudioConverter::InputCallback* input) {
    audio_converter_.RemoveInput(input);
  }

 private:
  virtual double ProvideInput(AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) override {
    audio_converter_.Convert(audio_bus);
    return 1.0;
  }

  AudioConverter audio_converter_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackAudioConverter);
};

VirtualAudioInputStream* VirtualAudioInputStream::MakeStream(
    AudioManagerBase* manager, const AudioParameters& params,
    base::MessageLoopProxy* message_loop) {
  return new VirtualAudioInputStream(manager, params, message_loop);
}

VirtualAudioInputStream::VirtualAudioInputStream(
    AudioManagerBase* manager, const AudioParameters& params,
    base::MessageLoopProxy* message_loop)
    : audio_manager_(manager),
      message_loop_(message_loop),
      callback_(NULL),
      buffer_duration_ms_(base::TimeDelta::FromMilliseconds(
          params.frames_per_buffer() * base::Time::kMillisecondsPerSecond /
          static_cast<float>(params.sample_rate()))),
      buffer_(new uint8[params.GetBytesPerBuffer()]),
      params_(params),
      audio_bus_(AudioBus::Create(params_)),
      mixer_(params_, params_, false),
      num_attached_outputs_streams_(0) {
}

VirtualAudioInputStream::~VirtualAudioInputStream() {
  for (AudioConvertersMap::iterator it = converters_.begin();
       it != converters_.end(); ++it)
    delete it->second;

  DCHECK_EQ(0, num_attached_outputs_streams_);
}

bool VirtualAudioInputStream::Open() {
  memset(buffer_.get(), 0, params_.GetBytesPerBuffer());
  return true;
}

void VirtualAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  callback_ = callback;
  on_more_data_cb_.Reset(base::Bind(&VirtualAudioInputStream::ReadAudio,
                                    base::Unretained(this)));
  audio_manager_->GetMessageLoop()->PostTask(FROM_HERE,
                                             on_more_data_cb_.callback());
}

void VirtualAudioInputStream::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  on_more_data_cb_.Cancel();
}

void VirtualAudioInputStream::AddOutputStream(
    VirtualAudioOutputStream* stream, const AudioParameters& output_params) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  AudioConvertersMap::iterator converter = converters_.find(output_params);
  if (converter == converters_.end()) {
    std::pair<AudioConvertersMap::iterator, bool> result = converters_.insert(
        std::make_pair(output_params,
                       new LoopbackAudioConverter(output_params, params_)));
    converter = result.first;

    // Add to main mixer if we just added a new AudioTransform.
    mixer_.AddInput(converter->second);
  }
  converter->second->AddInput(stream);
  ++num_attached_outputs_streams_;
}

void VirtualAudioInputStream::RemoveOutputStream(
    VirtualAudioOutputStream* stream, const AudioParameters& output_params) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(converters_.find(output_params) != converters_.end());
  converters_[output_params]->RemoveInput(stream);

  --num_attached_outputs_streams_;
}

void VirtualAudioInputStream::ReadAudio() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(callback_);

  mixer_.Convert(audio_bus_.get());
  audio_bus_->ToInterleaved(params_.frames_per_buffer(),
                            params_.bits_per_sample() / 8,
                            buffer_.get());

  callback_->OnData(this,
                    buffer_.get(),
                    params_.GetBytesPerBuffer(),
                    params_.GetBytesPerBuffer(),
                    1.0);

  message_loop_->PostDelayedTask(FROM_HERE,
                                 on_more_data_cb_.callback(),
                                 buffer_duration_ms_);
}

void VirtualAudioInputStream::Close() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (callback_) {
    DCHECK(on_more_data_cb_.IsCancelled());
    callback_->OnClose(this);
    callback_ = NULL;
  }
  audio_manager_->ReleaseInputStream(this);
}

double VirtualAudioInputStream::GetMaxVolume() {
  return 1.0;
}

void VirtualAudioInputStream::SetVolume(double volume) {}

double VirtualAudioInputStream::GetVolume() {
  return 1.0;
}

void VirtualAudioInputStream::SetAutomaticGainControl(bool enabled) {}

bool VirtualAudioInputStream::GetAutomaticGainControl() {
  return false;
}

}  // namespace media
