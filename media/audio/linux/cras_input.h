// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_CRAS_INPUT_H_
#define MEDIA_AUDIO_LINUX_CRAS_INPUT_H_

#include <cras_client.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/audio/audio_input_stream_impl.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerLinux;

// Provides an input stream for audio capture based on CRAS, the ChromeOS Audio
// Server.  This object is not thread safe and all methods should be invoked in
// the thread that created the object.
class CrasInputStream : public AudioInputStreamImpl {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasInputStream(const AudioParameters& params, AudioManagerLinux* manager);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~CrasInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() override;
  virtual void Start(AudioInputCallback* callback) override;
  virtual void Stop() override;
  virtual void Close() override;
  virtual double GetMaxVolume() override;
  virtual void SetVolume(double volume) override;
  virtual double GetVolume() override;

 private:
  // Handles requests to get samples from the provided buffer.  This will be
  // called by the audio server when it has samples ready.
  static int SamplesReady(cras_client* client,
                          cras_stream_id_t stream_id,
                          uint8* samples,
                          size_t frames,
                          const timespec* sample_ts,
                          void* arg);

  // Handles notificaiton that there was an error with the playback stream.
  static int StreamError(cras_client* client,
                         cras_stream_id_t stream_id,
                         int err,
                         void* arg);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback. Called from SamplesReady().
  void ReadAudio(size_t frames, uint8* buffer, const timespec* sample_ts);

  // Deals with an error that occured in the stream.  Called from StreamError().
  void NotifyStreamError(int err);

  // Convert from dB * 100 to a volume ratio.
  double GetVolumeRatioFromDecibels(double dB) const;

  // Convert from a volume ratio to dB.
  double GetDecibelsFromVolumeRatio(double volume_ratio) const;

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the audio
  // thread, which is owned by the audio manager and we don't want to addref
  // the manager from that thread.
  AudioManagerLinux* audio_manager_;

  // Size of frame in bytes.
  uint32 bytes_per_frame_;

  // Callback to pass audio samples too, valid while recording.
  AudioInputCallback* callback_;

  // The client used to communicate with the audio server.
  cras_client* client_;

  // PCM parameters for the stream.
  AudioParameters params_;

  // True if the stream has been started.
  bool started_;

  // ID of the playing stream.
  cras_stream_id_t stream_id_;

  DISALLOW_COPY_AND_ASSIGN(CrasInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_LINUX_ALSA_INPUT_H_
