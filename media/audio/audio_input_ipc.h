// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_IPC_H_
#define MEDIA_AUDIO_AUDIO_INPUT_IPC_H_

#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Contains IPC notifications for the state of the server side
// (AudioInputController) audio state changes and when an AudioInputController
// has been created.  Implemented by AudioInputDevice.
class MEDIA_EXPORT AudioInputIPCDelegate {
 public:
  // Valid states for the input stream.
  enum State {
    kRecording,
    kStopped,
    kError
  };

  // Called when an AudioInputController has been created.
  // The shared memory |handle| points to a memory section that's used to
  // transfer data between the AudioInputDevice and AudioInputController
  // objects.  The implementation of OnStreamCreated takes ownership.
  // The |socket_handle| is used by the AudioInputController to signal
  // notifications that more data is available and can optionally provide
  // parameter changes back.  The AudioInputDevice must read from this socket
  // and process the shared memory whenever data is read from the socket.
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               int length) = 0;

  // Called when state of an audio stream has changed.
  virtual void OnStateChanged(State state) = 0;

  // Called when the input stream volume has changed.
  virtual void OnVolume(double volume) = 0;

  // Called when a device has been started on the server side.
  // If the device could not be started, |device_id| will be empty.
  virtual void OnDeviceReady(const std::string& device_id) = 0;

  // Called when the AudioInputIPC object is going away and/or when the
  // IPC channel has been closed and no more IPC requests can be made.
  // Implementations must clear any references to the AudioInputIPC object
  // at this point.
  virtual void OnIPCClosed() = 0;

 protected:
  virtual ~AudioInputIPCDelegate();
};

// Provides IPC functionality for an AudioInputDevice.  The implementation
// should asynchronously deliver the messages to an AudioInputController object
// (or create one in the case of CreateStream()), that may live in a separate
// process.
class MEDIA_EXPORT AudioInputIPC {
 public:
  // Registers an AudioInputIPCDelegate and returns a |stream_id| that
  // must be used with all other IPC functions in this interface.
  virtual int AddDelegate(AudioInputIPCDelegate* delegate) = 0;

  // Unregisters a delegate that was previously registered via a call to
  // AddDelegate().  The audio stream should be in a closed state prior to
  // calling this function.
  virtual void RemoveDelegate(int stream_id) = 0;

  // Sends a request to create an AudioInputController object in the peer
  // process, identify it by |stream_id| and configure it to use the specified
  // audio |params|.  Once the stream has been created, the implementation must
  // generate a notification to the AudioInputIPCDelegate and call
  // OnStreamCreated().
  virtual void CreateStream(int stream_id, const AudioParameters& params,
      const std::string& device_id, bool automatic_gain_control) = 0;

  // Starts the device on the server side.  Once the device has started,
  // or failed to start, a callback to
  // AudioInputIPCDelegate::OnDeviceReady() must be made.
  virtual void StartDevice(int stream_id, int session_id) = 0;

  // Corresponds to a call to AudioInputController::Record() on the server side.
  virtual void RecordStream(int stream_id) = 0;

  // Sets the volume of the audio stream.
  virtual void SetVolume(int stream_id, double volume) = 0;

  // Closes the audio stream and deletes the matching AudioInputController
  // instance.  Prior to deleting the AudioInputController object, a call to
  // AudioInputController::Close must be made.
  virtual void CloseStream(int stream_id) = 0;

 protected:
  virtual ~AudioInputIPC();
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_IPC_H_
