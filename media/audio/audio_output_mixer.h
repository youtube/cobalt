// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioOutputMixer is a class that implements browser-side audio mixer.
// AudioOutputMixer implements both AudioOutputDispatcher and
// AudioSourceCallback interfaces.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_MIXER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_MIXER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"

namespace media {

class MEDIA_EXPORT AudioOutputMixer
    : public AudioOutputDispatcher,
      public AudioOutputStream::AudioSourceCallback {
 public:
  AudioOutputMixer(AudioManager* audio_manager,
                   const AudioParameters& params,
                   const base::TimeDelta& close_delay);

  // AudioOutputDispatcher interface.
  virtual bool OpenStream() override;
  virtual bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                           AudioOutputProxy* stream_proxy) override;
  virtual void StopStream(AudioOutputProxy* stream_proxy) override;
  virtual void StreamVolumeSet(AudioOutputProxy* stream_proxy,
                               double volume) override;
  virtual void CloseStream(AudioOutputProxy* stream_proxy) override;
  virtual void Shutdown() override;

  // AudioSourceCallback interface.
  virtual uint32 OnMoreData(uint8* dest,
                            uint32 max_size,
                            AudioBuffersState buffers_state) override;
  virtual void OnError(AudioOutputStream* stream, int code) override;
  virtual void WaitTillDataReady() override;

 private:
  friend class base::RefCountedThreadSafe<AudioOutputMixer>;
  virtual ~AudioOutputMixer();

  // Called by |close_timer_|. Closes physical stream.
  void ClosePhysicalStream();

  // The |lock_| must be acquired whenever we modify |proxies_| in the audio
  // manager thread or accessing it in the hardware audio thread. Read in the
  // audio manager thread is safe.
  base::Lock lock_;

  // List of audio output proxies currently being played.
  // For every proxy we store aux structure containing data necessary for
  // mixing.
  struct ProxyData {
    AudioOutputStream::AudioSourceCallback* audio_source_callback;
    double volume;
    int pending_bytes;
  };
  typedef std::map<AudioOutputProxy*, ProxyData> ProxyMap;
  ProxyMap proxies_;

  // Physical stream for this mixer.
  scoped_ptr<AudioOutputStream> physical_stream_;

  // Temporary buffer used when mixing. Allocated in the constructor
  // to avoid constant allocation/deallocation in the callback.
  scoped_array<uint8> mixer_data_;

  // Used to post delayed tasks to ourselves that we cancel inside Shutdown().
  base::WeakPtrFactory<AudioOutputMixer> weak_this_;
  base::DelayTimer<AudioOutputMixer> close_timer_;

  // Size of data in all in-flight buffers.
  int pending_bytes_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputMixer);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_MIXER_H_
