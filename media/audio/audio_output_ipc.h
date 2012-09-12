// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_

#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Contains IPC notifications for the state of the server side
// (AudioOutputController) audio state changes and when an AudioOutputController
// has been created.  Implemented by AudioOutputDevice.
class MEDIA_EXPORT AudioOutputIPCDelegate {
 public:
  // Current status of the audio output stream in the browser process. Browser
  // sends information about the current playback state and error to the
  // renderer process using this type.
  enum State {
    kPlaying,
    kPaused,
    kError
  };

  // Called when state of an audio stream has changed.
  virtual void OnStateChanged(State state) = 0;

  // Called when an audio stream has been created.
  // The shared memory |handle| points to a memory section that's used to
  // transfer audio buffers from the AudioOutputIPCDelegate back to the
  // AudioRendererHost.  The implementation of OnStreamCreated takes ownership.
  // The |socket_handle| is used by AudioRendererHost to signal requests for
  // audio data to be written into the shared memory. The AudioOutputIPCDelegate
  // must read from this socket and provide audio whenever data (search for
  // "pending_bytes") is received.
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               int length) = 0;

  // Called when the AudioOutputIPC object is going away and/or when the IPC
  // channel has been closed and no more ipc requests can be made.
  // Implementations must clear any references to the AudioOutputIPC object
  // at this point.
  virtual void OnIPCClosed() = 0;

 protected:
  virtual ~AudioOutputIPCDelegate();
};

// Provides IPC functionality for an AudioOutputDevice.  The implementation
// should asynchronously deliver the messages to an AudioOutputController object
// (or create one in the case of CreateStream()), that may live in a separate
// process.
class MEDIA_EXPORT AudioOutputIPC {
 public:
  // Registers an AudioOutputIPCDelegate and returns a |stream_id| that must
  // be used with all other IPC functions in this interface.
  virtual int AddDelegate(AudioOutputIPCDelegate* delegate) = 0;

  // Unregisters a delegate that was previously registered via a call to
  // AddDelegate().  The audio stream should be in a closed state prior to
  // calling this function.
  virtual void RemoveDelegate(int stream_id) = 0;

  // Sends a request to create an AudioOutputController object in the peer
  // process, identify it by |stream_id| and configure it to use the specified
  // audio |params| and number of synchronized input channels.
  // Once the stream has been created, the implementation must
  // generate a notification to the AudioOutputIPCDelegate and call
  // OnStreamCreated().
  virtual void CreateStream(int stream_id,
                            const AudioParameters& params,
                            int input_channels) = 0;

  // Starts playing the stream.  This should generate a call to
  // AudioOutputController::Play().
  virtual void PlayStream(int stream_id) = 0;

  // Pauses an audio stream.  This should generate a call to
  // AudioOutputController::Pause().
  virtual void PauseStream(int stream_id) = 0;

  // "Flushes" the audio device. This should generate a call to
  // AudioOutputController::Flush().
  // TODO(tommi): This is currently neither implemented nor called.  Remove?
  virtual void FlushStream(int stream_id) = 0;

  // Closes the audio stream and deletes the matching AudioOutputController
  // instance.  Prior to deleting the AudioOutputController object, a call to
  // AudioOutputController::Close must be made.
  virtual void CloseStream(int stream_id) = 0;

  // Sets the volume of the audio stream.
  virtual void SetVolume(int stream_id, double volume) = 0;

 protected:
  virtual ~AudioOutputIPC();
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_
