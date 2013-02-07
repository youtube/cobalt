// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates an output stream based on the cras (ChromeOS audio server) interface.
//
// CrasOutputStream object is *not* thread-safe and should only be used
// from the audio thread.

#ifndef MEDIA_AUDIO_LINUX_CRAS_OUTPUT_H_
#define MEDIA_AUDIO_LINUX_CRAS_OUTPUT_H_

#include <alsa/asoundlib.h>
#include <cras_client.h>
#include <ostream>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "media/audio/audio_io.h"

namespace media {

class AudioManagerLinux;
class AudioParameters;

// Implementation of AudioOuputStream for Chrome OS using the Chrome OS audio
// server.
class MEDIA_EXPORT CrasOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasOutputStream(const AudioParameters& params, AudioManagerLinux* manager);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~CrasOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  // Flags indicating the state of the stream.
  enum InternalState {
    kInError = 0,
    kCreated,
    kIsOpened,
    kIsPlaying,
    kIsStopped,
    kIsClosed
  };
  friend std::ostream& operator<<(std::ostream& os, InternalState);
  // Reports the current state for unit testing.
  InternalState state();

 private:
  // Handles requests to put samples in the provided buffer.  This will be
  // called by the audio server when it needs more data.
  static int PutSamples(cras_client* client,
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

  // Actually fills buffer with audio data.  Called from PutSamples().
  uint32 Render(size_t frames, uint8* buffer, const timespec* sample_ts);

  // Deals with an error that occured in the stream.  Called from StreamError().
  void NotifyStreamError(int err);

  // Functions to safeguard state transitions.  All changes to the object state
  // should go through these functions.
  bool CanTransitionTo(InternalState to);
  InternalState TransitionTo(InternalState to);

  // The client used to communicate with the audio server.
  cras_client* client_;

  // ID of the playing stream.
  cras_stream_id_t stream_id_;

  // Packet size in samples.
  uint32 samples_per_packet_;

  // Size of frame in bytes.
  uint32 bytes_per_frame_;

  // Rate in Hz.
  size_t frame_rate_;

  // Number of channels.
  size_t num_channels_;

  // PCM format for Alsa.
  const snd_pcm_format_t pcm_format_;

  // Current state.
  InternalState state_;

  // Volume level from 0.0 to 1.0.
  float volume_;

  // Audio manager that created us.  Used to report that we've been closed.
  AudioManagerLinux* manager_;

  // Callback to get audio samples.
  AudioSourceCallback* source_callback_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(CrasOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_LINUX_CRAS_OUTPUT_H_
