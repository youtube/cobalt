// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioOutputDispatcherImpl is an implementation of AudioOutputDispatcher.
//
// To avoid opening and closing audio devices more frequently than necessary,
// each dispatcher has a pool of inactive physical streams. A stream is closed
// only if it hasn't been used for a certain period of time (specified via the
// constructor).
//

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_IMPL_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_IMPL_H_

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"

class MessageLoop;

namespace media {

class AudioOutputProxy;

class MEDIA_EXPORT AudioOutputDispatcherImpl : public AudioOutputDispatcher {
 public:
  // |close_delay_ms| specifies delay after the stream is paused until
  // the audio device is closed.
  AudioOutputDispatcherImpl(AudioManager* audio_manager,
                            const AudioParameters& params,
                            const base::TimeDelta& close_delay);

  // Opens a new physical stream if there are no pending streams in
  // |idle_streams_|.  Do not call Close() or Stop() if this method fails.
  virtual bool OpenStream() override;

  // If there are pending streams in |idle_streams_| then it reuses one of
  // them, otherwise creates a new one.
  virtual bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                           AudioOutputProxy* stream_proxy) override;

  // Holds the physical stream temporarily in |pausing_streams_| and then
  // |stream| is  added to the pool of pending streams (i.e. |idle_streams_|).
  virtual void StopStream(AudioOutputProxy* stream_proxy) override;

  virtual void StreamVolumeSet(AudioOutputProxy* stream_proxy,
                               double volume) override;

  virtual void CloseStream(AudioOutputProxy* stream_proxy) override;

  virtual void Shutdown() override;

 private:
  typedef std::map<AudioOutputProxy*, AudioOutputStream*> AudioStreamMap;
  friend class base::RefCountedThreadSafe<AudioOutputDispatcherImpl>;
  virtual ~AudioOutputDispatcherImpl();

  friend class AudioOutputProxyTest;

  // Creates a new physical output stream, opens it and pushes to
  // |idle_streams_|.  Returns false if the stream couldn't be created or
  // opened.
  bool CreateAndOpenStream();

  // A task scheduled by StartStream(). Opens a new stream and puts
  // it in |idle_streams_|.
  void OpenTask();

  // Before a stream is reused, it should sit idle for a bit.  This task is
  // called once that time has elapsed.
  void StopStreamTask();

  // Called by |close_timer_|. Closes all pending streams.
  void ClosePendingStreams();

  base::TimeDelta pause_delay_;
  size_t paused_proxies_;
  typedef std::list<AudioOutputStream*> AudioOutputStreamList;
  AudioOutputStreamList idle_streams_;
  AudioOutputStreamList pausing_streams_;

  // Used to post delayed tasks to ourselves that we cancel inside Shutdown().
  base::WeakPtrFactory<AudioOutputDispatcherImpl> weak_this_;
  base::DelayTimer<AudioOutputDispatcherImpl> close_timer_;

  AudioStreamMap proxy_to_physical_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDispatcherImpl);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_IMPL_H_
