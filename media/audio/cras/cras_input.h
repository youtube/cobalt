// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_CRAS_INPUT_H_
#define MEDIA_AUDIO_CRAS_CRAS_INPUT_H_

#include <cras_client.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioManagerCrasBase;

// Provides an input stream for audio capture based on CRAS, the ChromeOS Audio
// Server.  This object is not thread safe and all methods should be invoked in
// the thread that created the object.
class MEDIA_EXPORT CrasInputStream : public AgcAudioStream<AudioInputStream> {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasInputStream(const AudioParameters& params,
                  AudioManagerCrasBase* manager,
                  const std::string& device_id);

  CrasInputStream(const CrasInputStream&) = delete;
  CrasInputStream& operator=(const CrasInputStream&) = delete;

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  ~CrasInputStream() override;

  // Implementation of AudioInputStream.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  // Handles requests to get samples from the provided buffer.  This will be
  // called by the audio server when it has samples ready.
  static int SamplesReady(struct libcras_stream_cb_data* data);

  // Handles notification that there was an error with the playback stream.
  static int StreamError(cras_client* client,
                         cras_stream_id_t stream_id,
                         int err,
                         void* arg);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback. Called from SamplesReady().
  void ReadAudio(size_t frames, uint8_t* buffer, const timespec* latency_ts);

  // Deals with an error that occured in the stream.  Called from StreamError().
  void NotifyStreamError(int err);

  // Convert from dB * 100 to a volume ratio.
  double GetVolumeRatioFromDecibels(double dB) const;

  // Convert from a volume ratio to dB.
  double GetDecibelsFromVolumeRatio(double volume_ratio) const;

  // Return true to use AEC in CRAS for this input stream.
  inline bool UseCrasAec() const;

  // Return true to use NS in CRAS for this input stream.
  inline bool UseCrasNs() const;

  // Return true to use AGC in CRAS for this input stream.
  inline bool UseCrasAgc() const;

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the audio
  // thread, which is owned by the audio manager and we don't want to addref
  // the manager from that thread.
  AudioManagerCrasBase* const audio_manager_;

  // Callback to pass audio samples too, valid while recording.
  AudioInputCallback* callback_;

  // The client used to communicate with the audio server.
  struct libcras_client* client_;

  // PCM parameters for the stream.
  const AudioParameters params_;

  // True if the stream has been started.
  bool started_;

  // ID of the playing stream.
  cras_stream_id_t stream_id_;

  // Direction of the stream.
  const CRAS_STREAM_DIRECTION stream_direction_;

  // Index of the CRAS device to stream input from.
  int pin_device_;

  // True if the stream is a system-wide loopback stream.
  bool is_loopback_;

  // True if we want to mute system audio during capturing.
  bool mute_system_audio_;
  bool mute_done_;

  // Value of input stream volume, between 0.0 - 1.0.
  double input_volume_;

  std::unique_ptr<AudioBus> audio_bus_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_INPUT_H_
