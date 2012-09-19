// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//  AudioOutputController                AudioOutputDevice
//           ^                                ^
//           |                                |
//           v               IPC              v
//    AudioRendererHost  <---------> AudioOutputIPC (AudioMessageFilter)
//
// Transportation of audio samples from the render to the browser process
// is done by using shared memory in combination with a sync socket pair
// to generate a low latency transport. The AudioOutputDevice user registers an
// AudioOutputDevice::RenderCallback at construction and will be polled by the
// AudioOutputDevice for audio to be played out by the underlying audio layers.
//
// State sequences.
//
//            Task [IO thread]                  IPC [IO thread]
//
// Start -> CreateStreamOnIOThread -----> CreateStream ------>
//       <- OnStreamCreated <- AudioMsg_NotifyStreamCreated <-
//       ---> PlayOnIOThread -----------> PlayStream -------->
//
// Optionally Play() / Pause() sequences may occur:
// Play -> PlayOnIOThread --------------> PlayStream --------->
// Pause -> PauseOnIOThread ------------> PauseStream -------->
// (note that Play() / Pause() sequences before OnStreamCreated are
//  deferred until OnStreamCreated, with the last valid state being used)
//
// AudioOutputDevice::Render => audio transport on audio thread =>
//                               |
// Stop --> ShutDownOnIOThread -------->  CloseStream -> Close
//
// This class utilizes several threads during its lifetime, namely:
// 1. Creating thread.
//    Must be the main render thread.
// 2. Control thread (may be the main render thread or another thread).
//    The methods: Start(), Stop(), Play(), Pause(), SetVolume()
//    must be called on the same thread.
// 3. IO thread (internal implementation detail - not exposed to public API)
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 4. Audio transport thread (See AudioDeviceThread).
//    Responsible for calling the AudioThreadCallback implementation that in
//    turn calls AudioRendererSink::RenderCallback which feeds audio samples to
//    the audio layer in the browser process using sync sockets and shared
//    memory.
//
// Implementation notes:
// - The user must call Stop() before deleting the class instance.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "media/base/media_export.h"
#include "media/audio/audio_device_thread.h"
#include "media/audio/audio_output_ipc.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/scoped_loop_observer.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

class MEDIA_EXPORT AudioOutputDevice
    : NON_EXPORTED_BASE(public AudioRendererSink),
      public AudioOutputIPCDelegate,
      NON_EXPORTED_BASE(public ScopedLoopObserver) {
 public:
  // AudioRendererSink implementation.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) OVERRIDE;
  virtual void InitializeIO(const AudioParameters& params,
                            int input_channels,
                            RenderCallback* callback) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;

  // Methods called on IO thread ----------------------------------------------
  // AudioOutputIPCDelegate methods.
  virtual void OnStateChanged(AudioOutputIPCDelegate::State state) OVERRIDE;
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               int length) OVERRIDE;
  virtual void OnIPCClosed() OVERRIDE;

  // Creates an uninitialized AudioOutputDevice. Clients must call Initialize()
  // before using.
  // TODO(tommi): When all dependencies on |content| have been removed
  // from AudioOutputDevice, move this class over to media/audio.
  AudioOutputDevice(AudioOutputIPC* ipc,
                    const scoped_refptr<base::MessageLoopProxy>& io_loop);

 protected:
  // Magic required by ref_counted.h to avoid any code deleting the object
  // accidentally while there are references to it.
  friend class base::RefCountedThreadSafe<AudioOutputDevice>;
  virtual ~AudioOutputDevice();

 private:
  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that needs to
  // be executed on that thread. They interact with AudioMessageFilter and
  // sends IPC messages on that thread.
  void CreateStreamOnIOThread(const AudioParameters& params,
                              int input_channels);
  void PlayOnIOThread();
  void PauseOnIOThread(bool flush);
  void ShutDownOnIOThread();
  void SetVolumeOnIOThread(double volume);

  // MessageLoop::DestructionObserver implementation for the IO loop.
  // If the IO loop dies before we do, we shut down the audio thread from here.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  AudioParameters audio_parameters_;

  // The number of optional synchronized input channels having the same
  // sample-rate and buffer-size as specified in audio_parameters_.
  int input_channels_;

  RenderCallback* callback_;

  // A pointer to the IPC layer that takes care of sending requests over to
  // the AudioRendererHost.
  AudioOutputIPC* ipc_;

  // Our stream ID on the message filter. Only accessed on the IO thread.
  // Must only be modified on the IO thread.
  int stream_id_;

  // State of Play() / Pause() calls before OnStreamCreated() is called.
  bool play_on_start_;

  // Set to |true| when OnStreamCreated() is called.
  // Set to |false| when ShutDownOnIOThread() is called.
  // This is for use with play_on_start_ to track Play() / Pause() state.
  // Must only be touched from the IO thread.
  bool is_started_;

  // Our audio thread callback class.  See source file for details.
  class AudioThreadCallback;

  // In order to avoid a race between OnStreamCreated and Stop(), we use this
  // guard to control stopping and starting the audio thread.
  base::Lock audio_thread_lock_;
  AudioDeviceThread audio_thread_;
  scoped_ptr<AudioOutputDevice::AudioThreadCallback> audio_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDevice);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_
