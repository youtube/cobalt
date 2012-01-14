// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioOutputDispatcher is a single-threaded class that dispatches creation and
// deletion of audio output streams. AudioOutputProxy objects use this class to
// allocate and recycle actual audio output streams. When playback is started,
// the proxy calls StreamStarted() to get an output stream that it uses to play
// audio. When playback is stopped, the proxy returns the stream back to the
// dispatcher by calling StreamStopped().
//
// To avoid opening and closing audio devices more frequently than necessary,
// each dispatcher has a pool of inactive physical streams. A stream is closed
// only if it hasn't been used for a certain period of time (specified via the
// constructor).
//
// AudioManagerBase creates one AudioOutputDispatcher on the audio thread for
// each possible set of audio parameters. I.e streams with different parameters
// are managed independently.  The AudioOutputDispatcher instance is then
// deleted on the audio thread when the AudioManager shuts down.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_H_

#include <vector>
#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_parameters.h"

class AudioOutputStream;
class MessageLoop;

class MEDIA_EXPORT AudioOutputDispatcher
    : public base::RefCountedThreadSafe<AudioOutputDispatcher> {
 public:
  // |close_delay_ms| specifies delay after the stream is paused until
  // the audio device is closed.
  AudioOutputDispatcher(AudioManager* audio_manager,
                        const AudioParameters& params,
                        base::TimeDelta close_delay);
  ~AudioOutputDispatcher();

  // Called by AudioOutputProxy when the stream is closed. Opens a new
  // physical stream if there are no pending streams in |idle_streams_|.
  // Returns false, if it fails to open it.
  bool StreamOpened();

  // Called by AudioOutputProxy when the stream is started. If there
  // are pending streams in |idle_streams_| then it returns one of them,
  // otherwise creates a new one. Returns a physical stream that must
  // be used, or NULL if it fails to open audio device. Ownership of
  // the result is passed to the caller.
  AudioOutputStream* StreamStarted();

  // Called by AudioOutputProxy when the stream is stopped.  Holds the
  // stream temporarily in |pausing_streams_| and then |stream| is
  // added to the pool of pending streams (i.e. |idle_streams_|).
  // Ownership of the |stream| is passed to the dispatcher.
  void StreamStopped(AudioOutputStream* stream);

  // Called by AudioOutputProxy when the stream is closed.
  void StreamClosed();

  // Called on the audio thread when the AudioManager is shutting down.
  void Shutdown();

  MessageLoop* message_loop();

 private:
  friend class AudioOutputProxyTest;

  // Creates a new physical output stream, opens it and pushes to
  // |idle_streams_|.  Returns false if the stream couldn't be created or
  // opened.
  bool CreateAndOpenStream();

  // A task scheduled by StreamStarted(). Opens a new stream and puts
  // it in |idle_streams_|.
  void OpenTask();

  // Before a stream is reused, it should sit idle for a bit.  This task is
  // called once that time has elapsed.
  void StopStreamTask();

  // Called by |close_timer_|. Closes all pending stream.
  void ClosePendingStreams();

  // A no-reference-held pointer (we don't want circular references) back to the
  // AudioManager that owns this object.
  AudioManager* audio_manager_;
  MessageLoop* message_loop_;
  AudioParameters params_;

  base::TimeDelta pause_delay_;
  size_t paused_proxies_;
  typedef std::list<AudioOutputStream*> AudioOutputStreamList;
  AudioOutputStreamList idle_streams_;
  AudioOutputStreamList pausing_streams_;

  // Used to post delayed tasks to ourselves that we cancel inside Shutdown().
  base::WeakPtrFactory<AudioOutputDispatcher> weak_this_;
  base::DelayTimer<AudioOutputDispatcher> close_timer_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDispatcher);
};

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_H_
