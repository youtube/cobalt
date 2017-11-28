// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
#define MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_

// NullAudioSink effectively uses an extra thread to "throw away" the
// audio data at a rate resembling normal playback speed.  It's just like
// decoding to /dev/null!
//
// NullAudioSink can also be used in situations where the client has no
// audio device or we haven't written an audio implementation for a particular
// platform yet.

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "media/base/audio_renderer_sink.h"

namespace media {
class AudioBus;

class MEDIA_EXPORT NullAudioSink
    : NON_EXPORTED_BASE(public AudioRendererSink) {
 public:
  NullAudioSink();

  // AudioRendererSink implementation.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) override;
  virtual void Start() override;
  virtual void Stop() override;
  virtual void Pause(bool flush) override;
  virtual void Play() override;
  virtual bool SetVolume(double volume) override;
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) override {}

  // Enables audio frame hashing and reinitializes the MD5 context.  Must be
  // called prior to Initialize().
  void StartAudioHashForTesting();

  // Returns the MD5 hash of all audio frames seen since the last reset.
  std::string GetAudioHashForTesting();

 protected:
  virtual ~NullAudioSink();

 private:
  // Audio thread task that periodically calls FillBuffer() to consume
  // audio data.
  void FillBufferTask();

  void SetPlaying(bool is_playing);

  // A buffer passed to FillBuffer to advance playback.
  scoped_ptr<AudioBus> audio_bus_;

  AudioParameters params_;
  bool initialized_;
  bool playing_;
  RenderCallback* callback_;

  // Separate thread used to throw away data.
  base::Thread thread_;
  base::Lock lock_;

  // Controls whether or not a running MD5 hash is computed for audio frames.
  bool hash_audio_for_testing_;
  scoped_array<base::MD5Context> md5_channel_contexts_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioSink);
};

}  // namespace media

#endif  // MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
