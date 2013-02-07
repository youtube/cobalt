// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_

#include <string>
#include "base/atomicops.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"

// An AudioInputController controls an AudioInputStream and records data
// from this input stream. The two main methods are Record() and Close() and
// they are both executed on the audio thread which is injected by the two
// alternative factory methods, Create() or CreateLowLatency().
//
// All public methods of AudioInputController are non-blocking.
//
// Here is a state diagram for the AudioInputController:
//
//                    .-->  [ Closed / Error ]  <--.
//                    |                            |
//                    |                            |
//               [ Created ]  ---------->  [ Recording ]
//                    ^
//                    |
//              *[  Empty  ]
//
// * Initial state
//
// State sequences (assuming low-latency):
//
//  [Creating Thread]                     [Audio Thread]
//
//      User               AudioInputController               EventHandler
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// CrateLowLatency() ==>        DoCreate()
//                   AudioManager::MakeAudioInputStream()
//                        AudioInputStream::Open()
//                                  .- - - - - - - - - - - - ->   OnError()
//                          create the data timer
//                                  .------------------------->  OnCreated()
//                               kCreated
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Record() ==>                 DoRecord()
//                      AudioInputStream::Start()
//                                  .------------------------->  OnRecording()
//                          start the data timer
//                              kRecording
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Close() ==>                  DoClose()
//                        delete the data timer
//                           state_ = kClosed
//                        AudioInputStream::Stop()
//                        AudioInputStream::Close()
//                          SyncWriter::Close()
// Closure::Run() <-----------------.
// (closure-task)
//
// The audio thread itself is owned by the AudioManager that the
// AudioInputController holds a reference to.  When performing tasks on the
// audio thread, the controller must not add or release references to the
// AudioManager or itself (since it in turn holds a reference to the manager).
//
namespace media {

class MEDIA_EXPORT AudioInputController
    : public base::RefCountedThreadSafe<AudioInputController>,
      public AudioInputStream::AudioInputCallback {
 public:
  // An event handler that receives events from the AudioInputController. The
  // following methods are all called on the audio thread.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual void OnCreated(AudioInputController* controller) = 0;
    virtual void OnRecording(AudioInputController* controller) = 0;
    virtual void OnError(AudioInputController* controller, int error_code) = 0;
    virtual void OnData(AudioInputController* controller, const uint8* data,
                        uint32 size) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous writer interface used by AudioInputController for
  // synchronous writing.
  class SyncWriter {
   public:
    virtual ~SyncWriter() {}

    // Notify the synchronous writer about the number of bytes in the
    // soundcard which has been recorded.
    virtual void UpdateRecordedBytes(uint32 bytes) = 0;

    // Write certain amount of data from |data|. This method returns
    // number of written bytes.
    virtual uint32 Write(const void* data, uint32 size, double volume) = 0;

    // Close this synchronous writer.
    virtual void Close() = 0;
  };

  // AudioInputController::Create() can use the currently registered Factory
  // to create the AudioInputController. Factory is intended for testing only.
  class Factory {
   public:
    virtual AudioInputController* Create(AudioManager* audio_manager,
                                         EventHandler* event_handler,
                                         AudioParameters params) = 0;
   protected:
    virtual ~Factory() {}
  };

  // Factory method for creating an AudioInputController.
  // The audio device will be created on the audio thread, and when that is
  // done, the event handler will receive an OnCreated() call from that same
  // thread.
  static scoped_refptr<AudioInputController> Create(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      const AudioParameters& params);

  // Sets the factory used by the static method Create(). AudioInputController
  // does not take ownership of |factory|. A value of NULL results in an
  // AudioInputController being created directly.
  static void set_factory_for_testing(Factory* factory) { factory_ = factory; }
  AudioInputStream* stream_for_testing() { return stream_; }

  // Factory method for creating an AudioInputController for low-latency mode.
  // The audio device will be created on the audio thread, and when that is
  // done, the event handler will receive an OnCreated() call from that same
  // thread.
  static scoped_refptr<AudioInputController> CreateLowLatency(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      const AudioParameters& params,
      const std::string& device_id,
      // External synchronous writer for audio controller.
      SyncWriter* sync_writer);

  // Starts recording using the created audio input stream.
  // This method is called on the creator thread.
  virtual void Record();

  // Closes the audio input stream. The state is changed and the resources
  // are freed on the audio thread. |closed_task| is then executed on the thread
  // that called Close().
  // Callbacks (EventHandler and SyncWriter) must exist until |closed_task|
  // is called.
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  // This method trampolines to the audio thread.
  virtual void Close(const base::Closure& closed_task);

  // Sets the capture volume of the input stream. The value 0.0 corresponds
  // to muted and 1.0 to maximum volume.
  virtual void SetVolume(double volume);

  // Sets the Automatic Gain Control (AGC) state of the input stream.
  // Changing the AGC state is not supported while recording is active.
  virtual void SetAutomaticGainControl(bool enabled);

  // AudioInputCallback implementation. Threading details depends on the
  // device-specific implementation.
  virtual void OnData(AudioInputStream* stream, const uint8* src, uint32 size,
                      uint32 hardware_delay_bytes, double volume) OVERRIDE;
  virtual void OnClose(AudioInputStream* stream) OVERRIDE;
  virtual void OnError(AudioInputStream* stream, int code) OVERRIDE;

  bool LowLatencyMode() const { return sync_writer_ != NULL; }

 protected:
  friend class base::RefCountedThreadSafe<AudioInputController>;

  // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kRecording,
    kClosed,
    kError
  };

  AudioInputController(EventHandler* handler, SyncWriter* sync_writer);
  virtual ~AudioInputController();

  // Methods called on the audio thread (owned by the AudioManager).
  void DoCreate(AudioManager* audio_manager, const AudioParameters& params,
                const std::string& device_id);
  void DoRecord();
  void DoClose();
  void DoReportError(int code);
  void DoSetVolume(double volume);
  void DoSetAutomaticGainControl(bool enabled);

  // Method which ensures that OnError() is triggered when data recording
  // times out. Called on the audio thread.
  void DoCheckForNoData();

  // Helper method that stops, closes, and NULL:s |*stream_|.
  // Signals event when done if the event is not NULL.
  void DoStopCloseAndClearStream(base::WaitableEvent* done);

  void SetDataIsActive(bool enabled);
  bool GetDataIsActive();

  // Gives access to the message loop of the creating thread.
  scoped_refptr<base::MessageLoopProxy> creator_loop_;

  // The message loop of audio-manager thread that this object runs on.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // Contains the AudioInputController::EventHandler which receives state
  // notifications from this class.
  EventHandler* handler_;

  // Pointer to the audio input stream object.
  AudioInputStream* stream_;

  // |no_data_timer_| is used to call OnError() when we stop receiving
  // OnData() calls without an OnClose() call. This can occur
  // when an audio input device is unplugged whilst recording on Windows.
  // See http://crbug.com/79936 for details.
  // This member is only touched by the audio thread.
  scoped_ptr<base::Timer> no_data_timer_;

  // This flag is used to signal that we are receiving OnData() calls, i.e,
  // that data is active. It can be touched by the audio thread and by the
  // low-level audio thread which calls OnData(). E.g. on Windows, the
  // low-level audio thread is called wasapi_capture_thread.
  base::subtle::Atomic32 data_is_active_;

  // |state_| is written on the audio thread and is read on the hardware audio
  // thread. These operations need to be locked. But lock is not required for
  // reading on the audio input controller thread.
  State state_;

  base::Lock lock_;

  // SyncWriter is used only in low-latency mode for synchronous writing.
  SyncWriter* sync_writer_;

  static Factory* factory_;

  double max_volume_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
