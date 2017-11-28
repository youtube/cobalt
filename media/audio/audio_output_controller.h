// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/base/media_export.h"

namespace base {
class WaitableEvent;
}  // namespace base

class MessageLoop;

// An AudioOutputController controls an AudioOutputStream and provides data
// to this output stream. It has an important function that it executes
// audio operations like play, pause, stop, etc. on a separate thread,
// namely the audio manager thread.
//
// All the public methods of AudioOutputController are non-blocking.
// The actual operations are performed on the audio manager thread.
//
// Here is a state diagram for the AudioOutputController:
//
//             .----------------------->  [ Closed / Error ]  <------.
//             |                                   ^                 |
//             |                                   |                 |
//        [ Created ]  -->  [ Starting ]  -->  [ Playing ]  -->  [ Paused ]
//             ^                 |                 ^                |  ^
//             |                 |                 |                |  |
//             |                 |                 `----------------'  |
//             |                 V                                     |
//             |        [ PausedWhenStarting ] ------------------------'
//             |
//       *[  Empty  ]
//
// * Initial state
//
// At any time after reaching the Created state but before Closed / Error, the
// AudioOutputController may be notified of a device change via OnDeviceChange()
// and transition to the Recreating state.  If OnDeviceChange() completes
// successfully the state will transition back to an equivalent pre-call state.
// E.g., if the state was Paused or PausedWhenStarting, the new state will be
// Created, since these states are all functionally equivalent and require a
// Play() call to continue to the next state.
//
// The AudioOutputStream can request data from the AudioOutputController via the
// AudioSourceCallback interface. AudioOutputController uses the SyncReader
// passed to it via construction to synchronously fulfill this read request.
//
// Since AudioOutputController uses AudioManager's message loop the controller
// uses WeakPtr to allow safe cancellation of pending tasks.
//

namespace media {

class MEDIA_EXPORT AudioOutputController
    : public base::RefCountedThreadSafe<AudioOutputController>,
      public AudioOutputStream::AudioSourceCallback,
      NON_EXPORTED_BASE(public AudioManager::AudioDeviceListener)  {
 public:
  // An event handler that receives events from the AudioOutputController. The
  // following methods are called on the audio manager thread.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual void OnCreated(AudioOutputController* controller) = 0;
    virtual void OnPlaying(AudioOutputController* controller) = 0;
    virtual void OnPaused(AudioOutputController* controller) = 0;
    virtual void OnError(AudioOutputController* controller, int error_code) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous reader interface used by AudioOutputController for
  // synchronous reading.
  // TODO(crogers): find a better name for this class and the Read() method
  // now that it can handle synchronized I/O.
  class SyncReader {
   public:
    virtual ~SyncReader() {}

    // Notify the synchronous reader the number of bytes in the
    // AudioOutputController not yet played. This is used by SyncReader to
    // prepare more data and perform synchronization.
    virtual void UpdatePendingBytes(uint32 bytes) = 0;

    // Attempt to completely fill |dest|, return the actual number of
    // frames that could be read.
    // |source| may optionally be provided for input data.
    virtual int Read(AudioBus* source, AudioBus* dest) = 0;

    // Close this synchronous reader.
    virtual void Close() = 0;

    // Check if data is ready.
    virtual bool DataReady() = 0;
  };

  // Factory method for creating an AudioOutputController.
  // This also creates and opens an AudioOutputStream on the audio manager
  // thread, and if this is successful, the |event_handler| will receive an
  // OnCreated() call from the same audio manager thread.  |audio_manager| must
  // outlive AudioOutputController.
  static scoped_refptr<AudioOutputController> Create(
      AudioManager* audio_manager, EventHandler* event_handler,
      const AudioParameters& params, SyncReader* sync_reader);

  // Methods to control playback of the stream.

  // Starts the playback of this audio output stream.
  void Play();

  // Pause this audio output stream.
  void Pause();

  // Discard all audio data buffered in this output stream. This method only
  // has effect when the stream is paused.
  void Flush();

  // Closes the audio output stream. The state is changed and the resources
  // are freed on the audio manager thread. closed_task is executed after that.
  // Callbacks (EventHandler and SyncReader) must exist until closed_task is
  // called.
  //
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  void Close(const base::Closure& closed_task);

  // Sets the volume of the audio output stream.
  void SetVolume(double volume);

  // AudioSourceCallback implementation.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) override;
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) override;
  virtual void OnError(AudioOutputStream* stream, int code) override;
  virtual void WaitTillDataReady() override;

  // AudioDeviceListener implementation.  When called AudioOutputController will
  // shutdown the existing |stream_|, transition to the kRecreating state,
  // create a new stream, and then transition back to an equivalent state prior
  // to being called.
  virtual void OnDeviceChange() override;

 protected:
    // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kPlaying,
    kStarting,
    kPausedWhenStarting,
    kPaused,
    kClosed,
    kError,
    kRecreating,
  };

  friend class base::RefCountedThreadSafe<AudioOutputController>;
  virtual ~AudioOutputController();

 private:
  // We are polling sync reader if data became available.
  static const int kPollNumAttempts;
  static const int kPollPauseInMilliseconds;

  AudioOutputController(AudioManager* audio_manager, EventHandler* handler,
                        const AudioParameters& params, SyncReader* sync_reader);

  // The following methods are executed on the audio manager thread.
  void DoCreate();
  void DoPlay();
  void PollAndStartIfDataReady();
  void DoPause();
  void DoFlush();
  void DoClose();
  void DoSetVolume(double volume);
  void DoReportError(int code);

  // Helper method that starts physical stream.
  void StartStream();

  // Helper method that stops, closes, and NULLs |*stream_|.
  // Signals event when done if it is not NULL.
  void DoStopCloseAndClearStream(base::WaitableEvent *done);

  AudioManager* audio_manager_;

  // |handler_| may be called only if |state_| is not kClosed.
  EventHandler* handler_;
  AudioOutputStream* stream_;

  // The current volume of the audio stream.
  double volume_;

  // |state_| is written on the audio manager thread and is read on the
  // hardware audio thread. These operations need to be locked. But lock
  // is not required for reading on the audio manager thread.
  State state_;

  // The |lock_| must be acquired whenever we access |state_| from a thread
  // other than the audio manager thread.
  base::Lock lock_;

  // SyncReader is used only in low latency mode for synchronous reading.
  SyncReader* sync_reader_;

  // The message loop of audio manager thread that this object runs on.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // When starting stream we wait for data to become available.
  // Number of times left.
  int number_polling_attempts_left_;

  AudioParameters params_;

  // Used to post delayed tasks to ourselves that we can cancel.
  // We don't want the tasks to hold onto a reference as it will slow down
  // shutdown and force it to wait for the most delayed task.
  // Also, if we're shutting down, we do not want to poll for more data.
  base::WeakPtrFactory<AudioOutputController> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
