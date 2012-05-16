// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"

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
// The AudioOutputStream can request data from the AudioOutputController via the
// AudioSourceCallback interface. AudioOutputController uses the SyncReader
// passed to it via construction to synchronously fulfill this read request.
//
// The audio manager thread is owned by the AudioManager that the
// AudioOutputController holds a reference to.  When performing tasks on this
// thread, the controller must not add or release references to the
// AudioManager or itself (since it in turn holds a reference to the manager),
// for delayed tasks as it can slow down or even prevent normal shut down.
// So, for tasks on the audio thread, the controller uses WeakPtr which enables
// us to safely cancel pending polling tasks.
//
// AudioManager will take care of properly shutting down the audio manager
// thread.
//
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioOutputController
    : public base::RefCountedThreadSafe<AudioOutputController>,
      public AudioOutputStream::AudioSourceCallback {
 public:
  // Value sent by the controller to the renderer in low-latency mode
  // indicating that the stream is paused.
  static const int kPauseMark;

  // An event handler that receives events from the AudioOutputController. The
  // following methods are called on the audio manager thread.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual ~EventHandler() {}
    virtual void OnCreated(AudioOutputController* controller) = 0;
    virtual void OnPlaying(AudioOutputController* controller) = 0;
    virtual void OnPaused(AudioOutputController* controller) = 0;
    virtual void OnError(AudioOutputController* controller, int error_code) = 0;
  };

  // A synchronous reader interface used by AudioOutputController for
  // synchronous reading.
  class SyncReader {
   public:
    virtual ~SyncReader() {}

    // Notify the synchronous reader the number of bytes in the
    // AudioOutputController not yet played. This is used by SyncReader to
    // prepare more data and perform synchronization.
    virtual void UpdatePendingBytes(uint32 bytes) = 0;

    // Read certain amount of data into |data|. This method returns if some
    // data is available.
    virtual uint32 Read(void* data, uint32 size) = 0;

    // Close this synchronous reader.
    virtual void Close() = 0;

    // Poll if data is ready.
    // Not reliable, as there is no guarantee that renderer is "new-style"
    // renderer that writes metadata into buffer. After several unsuccessful
    // attempts caller should assume the data is ready even if that function
    // returns false.
    virtual bool DataReady() = 0;
  };

  // Factory method for creating an AudioOutputController.
  // This also creates and opens an AudioOutputStream on the audio manager
  // thread, and if this is successful, the |event_handler| will receive an
  // OnCreated() call from the same audio manager thread.
  static scoped_refptr<AudioOutputController> Create(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      const AudioParameters& params,
      // External synchronous reader for audio controller.
      SyncReader* sync_reader);

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

  ///////////////////////////////////////////////////////////////////////////
  // AudioSourceCallback methods.
  virtual uint32 OnMoreData(uint8* dest,
                            uint32 max_size,
                            AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnError(AudioOutputStream* stream, int code) OVERRIDE;
  virtual void WaitTillDataReady() OVERRIDE;

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
  };

  friend class base::RefCountedThreadSafe<AudioOutputController>;
  virtual ~AudioOutputController();

 private:
  // We are polling sync reader if data became available.
  static const int kPollNumAttempts;
  static const int kPollPauseInMilliseconds;

  AudioOutputController(EventHandler* handler,
                        SyncReader* sync_reader);

  // The following methods are executed on the audio manager thread.
  void DoCreate(AudioManager* audio_manager, const AudioParameters& params);
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

  // Used to post delayed tasks to ourselves that we can cancel.
  // We don't want the tasks to hold onto a reference as it will slow down
  // shutdown and force it to wait for the most delayed task.
  // Also, if we're shutting down, we do not want to poll for more data.
  base::WeakPtrFactory<AudioOutputController> weak_this_;

  // Workaround for Mac OS X bug, see crbug.com/128128.
  base::Time previous_stop_time_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
