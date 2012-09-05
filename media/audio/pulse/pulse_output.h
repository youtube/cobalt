// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates an audio output stream based on the PulseAudio asynchronous API.
//
// If the stream is successfully opened, Close() must be called before the
// stream is deleted as Close() is responsible for ensuring resource cleanup
// occurs.
//
// This object is designed so that all AudioOutputStream methods will be called
// on the same thread that created the object.
//
// WARNING: This object blocks on internal PulseAudio calls in Open() while
// waiting for PulseAudio's context structure to be ready.  It also blocks in
// inside PulseAudio in Start() and repeated during playback, waiting for
// PulseAudio write callbacks to occur.

#ifndef MEDIA_AUDIO_PULSE_PULSE_OUTPUT_H_
#define MEDIA_AUDIO_PULSE_PULSE_OUTPUT_H_

#include <pulse/pulseaudio.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/audio/audio_io.h"
#include "media/base/channel_layout.h"

namespace media {

#if defined(OS_LINUX)
class AudioManagerLinux;
typedef AudioManagerLinux AudioManagerPulse;
#elif defined(OS_OPENBSD)
class AudioManagerOpenBSD;
typedef AudioManagerOpenBSD AudioManagerPulse;
#else
#error Unsupported platform
#endif

class AudioParameters;
class SeekableBuffer;

class PulseAudioOutputStream : public AudioOutputStream {
 public:
  PulseAudioOutputStream(const AudioParameters& params,
                         AudioManagerPulse* manager);

  virtual ~PulseAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  // PulseAudio Callbacks.
  static void ContextStateCallback(pa_context* context, void* state_addr);
  static void WriteRequestCallback(pa_stream* playback_handle, size_t length,
                                   void* stream_addr);

  // Iterate the PulseAudio mainloop to get write requests.
  void WaitForWriteRequest();

  // Get another packet from the data source and write it to the client buffer.
  bool BufferPacketFromSource();

  // Fulfill a write request from the write request callback.  If the write
  // can't be finished a first, post a new attempt to the message loop.
  void FulfillWriteRequest(size_t requested_bytes);

  // Write data from the client buffer to the PulseAudio stream.
  void WriteToStream(size_t bytes_to_write, size_t* bytes_written);

  // API for Proxying calls to the AudioSourceCallback provided during Start().
  int RunDataCallback(AudioBus* audio_bus, AudioBuffersState buffers_state);

  // Close() helper function to free internal structs.
  void Reset();

  // Configuration constants from the constructor.  Referencable by all threads
  // since they are constants.
  const ChannelLayout channel_layout_;
  const uint32 channel_count_;
  const pa_sample_format_t sample_format_;
  const uint32 sample_rate_;
  const uint32 bytes_per_frame_;

  // Audio manager that created us.  Used to report that we've closed.
  AudioManagerPulse* manager_;

  // PulseAudio API structs.
  pa_context* pa_context_;
  pa_mainloop* pa_mainloop_;

  // Handle to the actual PulseAudio playback stream.
  pa_stream* playback_handle_;

  // Device configuration data.  Populated after Open() completes.
  uint32 packet_size_;
  uint32 frames_per_packet_;

  // Client side audio buffer feeding pulse audio's server side buffer.
  scoped_ptr<media::SeekableBuffer> client_buffer_;

  // Float representation of volume from 0.0 to 1.0.
  float volume_;

  // Flag indicating the code should stop reading from the data source or
  // writing to the PulseAudio server.  This is set because the device has
  // entered an unrecoverable error state, or the Close() has executed.
  bool stream_stopped_;

  // Whether or not PulseAudio has called the WriteCallback for the most recent
  // set of pa_mainloop iterations.
  bool write_callback_handled_;

  // Allows us to run tasks on the PulseAudioOutputStream instance which are
  // bound by its lifetime.
  base::WeakPtrFactory<PulseAudioOutputStream> weak_factory_;

  // Callback to audio data source.
  AudioSourceCallback* source_callback_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_OUTPUT_H_
