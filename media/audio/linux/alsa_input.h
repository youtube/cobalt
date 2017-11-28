// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_ALSA_INPUT_H_
#define MEDIA_AUDIO_LINUX_ALSA_INPUT_H_

#include <alsa/asoundlib.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "media/audio/audio_input_stream_impl.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AlsaWrapper;
class AudioManagerLinux;

// Provides an input stream for audio capture based on the ALSA PCM interface.
// This object is not thread safe and all methods should be invoked in the
// thread that created the object.
class AlsaPcmInputStream : public AudioInputStreamImpl {
 public:
  // Pass this to the constructor if you want to attempt auto-selection
  // of the audio recording device.
  static const char* kAutoSelectDevice;

  // Create a PCM Output stream for the ALSA device identified by
  // |device_name|. If unsure of what to use for |device_name|, use
  // |kAutoSelectDevice|.
  AlsaPcmInputStream(AudioManagerLinux* audio_manager,
                     const std::string& device_name,
                     const AudioParameters& params,
                     AlsaWrapper* wrapper);

  virtual ~AlsaPcmInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() override;
  virtual void Start(AudioInputCallback* callback) override;
  virtual void Stop() override;
  virtual void Close() override;
  virtual double GetMaxVolume() override;
  virtual void SetVolume(double volume) override;
  virtual double GetVolume() override;

 private:
  // Logs the error and invokes any registered callbacks.
  void HandleError(const char* method, int error);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback and schedules the next read.
  void ReadAudio();

  // Recovers from any device errors if possible.
  bool Recover(int error);

  // Utility function for talking with the ALSA API.
  snd_pcm_sframes_t GetCurrentDelay();

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the audio
  // thread, which is owned by the audio manager and we don't want to addref
  // the manager from that thread.
  AudioManagerLinux* audio_manager_;
  std::string device_name_;
  AudioParameters params_;
  int bytes_per_buffer_;
  AlsaWrapper* wrapper_;
  int buffer_duration_ms_;  // Length of each recorded buffer in milliseconds.
  AudioInputCallback* callback_;  // Valid during a recording session.
  base::Time next_read_time_;  // Scheduled time for the next read callback.
  snd_pcm_t* device_handle_;  // Handle to the ALSA PCM recording device.
  snd_mixer_t* mixer_handle_; // Handle to the ALSA microphone mixer.
  snd_mixer_elem_t* mixer_element_handle_; // Handle to the capture element.
  base::WeakPtrFactory<AlsaPcmInputStream> weak_factory_;
  scoped_array<uint8> audio_buffer_;  // Buffer used for reading audio data.
  bool read_callback_behind_schedule_;

  DISALLOW_COPY_AND_ASSIGN(AlsaPcmInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_LINUX_ALSA_INPUT_H_
