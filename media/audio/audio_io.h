// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_IO_H_
#define MEDIA_AUDIO_AUDIO_IO_H_

#include "base/basictypes.h"
#include "media/audio/audio_buffers_state.h"

// Low-level audio output support. To make sound there are 3 objects involved:
// - AudioSource : produces audio samples on a pull model. Implements
//   the AudioSourceCallback interface.
// - AudioOutputStream : uses the AudioSource to render audio on a given
//   channel, format and sample frequency configuration. Data from the
//   AudioSource is delivered in a 'pull' model.
// - AudioManager : factory for the AudioOutputStream objects, manager
//   of the hardware resources and mixer control.
//
// The number and configuration of AudioOutputStream does not need to match the
// physically available hardware resources. For example you can have:
//
//  MonoPCMSource1 --> MonoPCMStream1 --> |       | --> audio left channel
//  StereoPCMSource -> StereoPCMStream -> | mixer |
//  MonoPCMSource2 --> MonoPCMStream2 --> |       | --> audio right channel
//
// This facility's objective is mix and render audio with low overhead using
// the OS basic audio support, abstracting as much as possible the
// idiosyncrasies of each platform. Non-goals:
// - Positional, 3d audio
// - Dependence on non-default libraries such as DirectX 9, 10, XAudio
// - Digital signal processing or effects
// - Extra features if a specific hardware is installed (EAX, X-fi)
//
// The primary client of this facility is audio coming from several tabs.
// Specifically for this case we avoid supporting complex formats such as MP3
// or WMA. Complex format decoding should be done by the renderers.


// Models an audio stream that gets rendered to the audio hardware output.
// Because we support more audio streams than physically available channels
// a given AudioOutputStream might or might not talk directly to hardware.
class AudioOutputStream {
 public:
  // Audio sources must implement AudioSourceCallback. This interface will be
  // called in a random thread which very likely is a high priority thread. Do
  // not rely on using this thread TLS or make calls that alter the thread
  // itself such as creating Windows or initializing COM.
  class AudioSourceCallback {
   public:
    virtual ~AudioSourceCallback() {}

    // Provide more data by filling |dest| up to |max_size| bytes. The provided
    // buffer size is usually what is specified in Open(). The source
    // will return the number of bytes it filled. The expected structure of
    // |dest| is platform and format specific.
    // |pending_bytes| is the number of bytes will be played before the
    // requested data is played.
    virtual uint32 OnMoreData(
        AudioOutputStream* stream, uint8* dest, uint32 max_size,
        AudioBuffersState buffers_state) = 0;

    // The stream is done with this callback. After this call the audio source
    // can go away or be destroyed.
    virtual void OnClose(AudioOutputStream* stream) = 0;

    // There was an error while playing a buffer. Audio source cannot be
    // destroyed yet. No direct action needed by the AudioStream, but it is
    // a good place to stop accumulating sound data since is is likely that
    // playback will not continue. |code| is an error code that is platform
    // specific.
    virtual void OnError(AudioOutputStream* stream, int code) = 0;
  };

  // Open the stream. |packet_size| is the requested buffer allocation which
  // the audio source thinks it can usually fill without blocking. Internally
  // two or three buffers of |packet_size| size are created, one will be
  // locked for playback and one will be ready to be filled in the call to
  // AudioSourceCallback::OnMoreData().
  // The number of buffers is controlled by AUDIO_PCM_LOW_LATENCY. See more
  // information below.
  //
  // TODO(ajwong): Streams are not reusable, so try to move packet_size into the
  // constructor.
  virtual bool Open(uint32 packet_size) = 0;

  // Starts playing audio and generating AudioSourceCallback::OnMoreData().
  // Since implementor of AudioOutputStream may have internal buffers, right
  // after calling this method initial buffers are fetched.
  //
  // The output stream does not take ownership of this callback.
  virtual void Start(AudioSourceCallback* callback) = 0;

  // Stops playing audio. Effect might not be instantaneous as the hardware
  // might have locked audio data that is processing.
  virtual void Stop() = 0;

  // Sets the relative volume, with range [0.0, 1.0] inclusive.
  virtual void SetVolume(double volume) = 0;

  // Gets the relative volume, with range [0.0, 1.0] inclusive.
  virtual void GetVolume(double* volume) = 0;

  // Close the stream. This also generates AudioSourceCallback::OnClose().
  // After calling this method, the object should not be used anymore.
  virtual void Close() = 0;

 protected:
  virtual ~AudioOutputStream() {}
};

// Models an audio sink receiving recorded audio from the audio driver.
class AudioInputStream {
 public:
  class AudioInputCallback {
   public:
    virtual ~AudioInputCallback() {}

    // Called by the audio recorder when a full packet of audio data is
    // available. This is called from a special audio thread and the
    // implementation should return as soon as possible.
    virtual void OnData(AudioInputStream* stream, const uint8* src,
                        uint32 size) = 0;

    // The stream is done with this callback, the last call received by this
    // audio sink.
    virtual void OnClose(AudioInputStream* stream) = 0;

    // There was an error while recording audio. The audio sink cannot be
    // destroyed yet. No direct action needed by the AudioInputStream, but it
    // is a good place to stop accumulating sound data since is is likely that
    // recording will not continue. |code| is an error code that is platform
    // specific.
    virtual void OnError(AudioInputStream* stream, int code) = 0;
  };

  // Open the stream and prepares it for recording. Call Start() to actually
  // begin recording.
  virtual bool Open() = 0;

  // Starts recording audio and generating AudioInputCallback::OnData().
  // The input stream does not take ownership of this callback.
  virtual void Start(AudioInputCallback* callback) = 0;

  // Stops recording audio. Effect might not be instantaneous as there could be
  // pending audio callbacks in the queue which will be issued first before
  // recording stops.
  virtual void Stop() = 0;

  // Close the stream. This also generates AudioInputCallback::OnClose(). This
  // should be the last call made on this object.
  virtual void Close() = 0;

 protected:
  virtual ~AudioInputStream() {}
};

#endif  // MEDIA_AUDIO_AUDIO_IO_H_
