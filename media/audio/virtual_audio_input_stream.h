// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_

#include <map>
#include <set>

#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"

namespace media {

class LoopbackAudioConverter;
class VirtualAudioOutputStream;

// VirtualAudioInputStream converts and mixes audio from attached
// VirtualAudioOutputStreams into a single stream. It will continuously render
// audio until this VirtualAudioInputStream is stopped and closed.
class MEDIA_EXPORT VirtualAudioInputStream : public AudioInputStream {
 public:
  static VirtualAudioInputStream* MakeStream(
      AudioManagerBase* manager,
      const AudioParameters& params,
      base::MessageLoopProxy* message_loop);

  virtual ~VirtualAudioInputStream();

  // AudioInputStream:
  virtual bool Open() override;
  virtual void Start(AudioInputCallback* callback) override;
  virtual void Stop() override;
  virtual void Close() override;
  virtual double GetMaxVolume() override;
  virtual void SetVolume(double volume) override;
  virtual double GetVolume() override;
  virtual void SetAutomaticGainControl(bool enabled) override;
  virtual bool GetAutomaticGainControl() override;

  // Attaches a VirtualAudioOutputStream to be used as input. This
  // VirtualAudioInputStream must outlive all attached streams, so any attached
  // stream must be closed (which causes a detach) before
  // VirtualAudioInputStream is destroyed.
  virtual void AddOutputStream(VirtualAudioOutputStream* stream,
                               const AudioParameters& output_params);

  // Detaches a VirtualAudioOutputStream and removes it as input.
  virtual void RemoveOutputStream(VirtualAudioOutputStream* stream,
                                  const AudioParameters& output_params);

 protected:
  friend class VirtualAudioInputStreamTest;
  FRIEND_TEST_ALL_PREFIXES(AudioOutputControllerTest,
                           VirtualStreamsTriggerDeviceChange);

  typedef std::map<AudioParameters, LoopbackAudioConverter*> AudioConvertersMap;

  VirtualAudioInputStream(AudioManagerBase* manager,
                          const AudioParameters& params,
                          base::MessageLoopProxy* message_loop);

  // When Start() is called on this class, we continuously schedule this
  // callback to render audio using any attached VirtualAudioOutputStreams until
  // Stop() is called.
  void ReadAudio();

  AudioManagerBase* audio_manager_;
  base::MessageLoopProxy* message_loop_;
  AudioInputCallback* callback_;

  // Non-const for testing.
  base::TimeDelta buffer_duration_ms_;
  scoped_array<uint8> buffer_;
  AudioParameters params_;
  scoped_ptr<AudioBus> audio_bus_;
  base::CancelableClosure on_more_data_cb_;

  // AudioConverters associated with the attached VirtualAudioOutputStreams,
  // partitioned by common AudioParameters.
  AudioConvertersMap converters_;

  // AudioConverter that takes all the audio converters and mixes them into one
  // final audio stream.
  AudioConverter mixer_;

  // Number of currently attached VirtualAudioOutputStreams.
  int num_attached_outputs_streams_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_
