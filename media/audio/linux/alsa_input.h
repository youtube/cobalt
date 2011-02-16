// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_ALSA_INPUT_H_
#define MEDIA_AUDIO_LINUX_ALSA_INPUT_H_

#include <alsa/asoundlib.h>

#include <string>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class AlsaWrapper;

// Provides an input stream for audio capture based on the ALSA PCM interface.
// This object is not thread safe and all methods should be invoked in the
// thread that created the object.
class AlsaPcmInputStream : public AudioInputStream {
 public:
  // Pass this to the constructor if you want to attempt auto-selection
  // of the audio recording device.
  static const char* kAutoSelectDevice;

  // Create a PCM Output stream for the ALSA device identified by
  // |device_name|. If unsure of what to use for |device_name|, use
  // |kAutoSelectDevice|.
  AlsaPcmInputStream(const std::string& device_name,
                     const AudioParameters& params,
                     AlsaWrapper* wrapper);
  virtual ~AlsaPcmInputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open();
  virtual void Start(AudioInputCallback* callback);
  virtual void Stop();
  virtual void Close();

 private:
  // Logs the error and invokes any registered callbacks.
  void HandleError(const char* method, int error);

  // Reads one or more packets of audio from the device, passes on to the
  // registered callback and schedules the next read.
  void ReadAudio();

  // Recovers from any device errors if possible.
  bool Recover(int error);

  std::string device_name_;
  AudioParameters params_;
  int bytes_per_packet_;
  AlsaWrapper* wrapper_;
  int packet_duration_ms_;  // Length of each recorded packet in milliseconds.
  AudioInputCallback* callback_;  // Valid during a recording session.
  base::Time next_read_time_;  // Scheduled time for the next read callback.
  snd_pcm_t* device_handle_;  // Handle to the ALSA PCM recording device.
  ScopedRunnableMethodFactory<AlsaPcmInputStream> task_factory_;
  scoped_array<uint8> audio_packet_;  // Buffer used for reading audio data.

  DISALLOW_COPY_AND_ASSIGN(AlsaPcmInputStream);
};

#endif  // MEDIA_AUDIO_LINUX_ALSA_INPUT_H_
