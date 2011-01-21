// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"

// An AudioInputController controls an AudioInputStream and records data
// from this input stream. It has an important function that it executes
// audio operations like record, pause, stop, etc. on a separate thread.
//
// All the public methods of AudioInputController are non-blocking except
// close, the actual operations are performed on the audio input controller
// thread.
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
namespace media {

class AudioInputController :
    public base::RefCountedThreadSafe<AudioInputController>,
    public AudioInputStream::AudioInputCallback {
 public:
  // An event handler that receives events from the AudioInputController. The
  // following methods are called on the audio input controller thread.
  class EventHandler {
   public:
    virtual ~EventHandler() {}
    virtual void OnCreated(AudioInputController* controller) = 0;
    virtual void OnRecording(AudioInputController* controller) = 0;
    virtual void OnError(AudioInputController* controller, int error_code) = 0;
    virtual void OnData(AudioInputController* controller, const uint8* data,
                        uint32 size) = 0;
  };

  // AudioInputController::Create uses the currently registered Factory to
  // create the AudioInputController. Factory is intended for testing.
  class Factory {
   public:
    virtual AudioInputController* Create(EventHandler* event_handler,
                                         AudioParameters params) = 0;

   protected:
    virtual ~Factory() {}
  };

  virtual ~AudioInputController();

  // Factory method for creating an AudioInputController.
  // If successful, an audio input controller thread is created. The audio
  // device will be created on the new thread and when that is done event
  // handler will receive a OnCreated() call.
  static scoped_refptr<AudioInputController> Create(
      EventHandler* event_handler,
      AudioParameters params);

  // Sets the factory used by the static method Create. AudioInputController
  // does not take ownership of |factory|. A value of NULL results in an
  // AudioInputController being created directly.
#if defined(UNIT_TEST)
  static void set_factory(Factory* factory) { factory_ = factory; }
#endif

  // Starts recording in this audio input stream.
  virtual void Record();

  // Closes the audio input stream and shutdown the audio input controller
  // thread. This method returns only after all operations are completed. This
  // input controller cannot be used after this method is called.
  //
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  virtual void Close();

  ///////////////////////////////////////////////////////////////////////////
  // AudioInputCallback methods.
  virtual void OnData(AudioInputStream* stream, const uint8* src, uint32 size);
  virtual void OnClose(AudioInputStream* stream);
  virtual void OnError(AudioInputStream* stream, int code);

 protected:
  // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kRecording,
    kClosed,
    kError
  };

  AudioInputController(EventHandler* handler);

  // The following methods are executed on the audio controller thread.
  void DoCreate(AudioParameters params);
  void DoRecord();
  void DoClose();
  void DoReportError(int code);

  EventHandler* handler_;
  AudioInputStream* stream_;

  // |state_| is written on the audio input controller thread and is read on
  // the hardware audio thread. These operations need to be locked. But lock
  // is not required for reading on the audio input controller thread.
  State state_;

  base::Lock lock_;

  // The audio input controller thread that this object runs on.
  base::Thread thread_;

  static Factory* factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputController);
};

}  // namespace media

#endif  //  MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
