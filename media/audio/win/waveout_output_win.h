// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

#include "base/basictypes.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class AudioManagerWin;

// Implements PCM audio output support for Windows using the WaveXXX API.
// While not as nice as the DirectSound-based API, it should work in all target
// operating systems regardless or DirectX version installed. It is known that
// in some machines WaveXXX based audio is better while in others DirectSound
// is better.
//
// Important: the OnXXXX functions in AudioSourceCallback are called by more
// than one thread so it is important to have some form of synchronization if
// you are keeping state in it.
class PCMWaveOutAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object and |device_id| which
  // is provided by the operating system.
  PCMWaveOutAudioOutputStream(AudioManagerWin* manager,
                              const AudioParameters& params,
                              int num_buffers,
                              UINT device_id);
  virtual ~PCMWaveOutAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open();
  virtual void Close();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);

  // Sends a buffer to the audio driver for playback.
  void QueueNextPacket(WAVEHDR* buffer);

 private:
  enum State {
    PCMA_BRAND_NEW,    // Initial state.
    PCMA_READY,        // Device obtained and ready to play.
    PCMA_PLAYING,      // Playing audio.
    PCMA_CLOSED        // Device has been released.
  };

  // Windows calls us back to feed more data to the device here. See msdn
  // documentation for 'waveOutProc' for details about the parameters.
  static void CALLBACK WaveCallback(HWAVEOUT hwo, UINT msg, DWORD_PTR instance,
                                    DWORD_PTR param1, DWORD_PTR param2);

  // If windows reports an error this function handles it and passes it to
  // the attached AudioSourceCallback::OnError().
  void HandleError(MMRESULT error);
  // Allocates and prepares the memory that will be used for playback. Only
  // two buffers are created.
  void SetupBuffers();
  // Deallocates the memory allocated in SetupBuffers.
  void FreeBuffers();

  // Reader beware. Visual C has stronger guarantees on volatile vars than
  // most people expect. In fact, it has release semantics on write and
  // acquire semantics on reads. See the msdn documentation.
  volatile State state_;

  // The audio manager that created this output stream. We notify it when
  // we close so it can release its own resources.
  AudioManagerWin* manager_;

  // We use the callback mostly to periodically request more audio data.
  AudioSourceCallback* callback_;

  // The number of buffers of size |buffer_size_| each to use.
  const int num_buffers_;

  // The size in bytes of each audio buffer, we usually have two of these.
  uint32 buffer_size_;

  // Volume level from 0 to 1.
  float volume_;

  // Channels from 0 to 8.
  const int channels_;

  // Number of bytes yet to be played in the hardware buffer.
  uint32 pending_bytes_;

  // The id assigned by the operating system to the selected wave output
  // hardware device. Usually this is just -1 which means 'default device'.
  UINT device_id_;

  // Windows native structure to encode the format parameters.
  WAVEFORMATPCMEX format_;

  // Handle to the instance of the wave device.
  HWAVEOUT waveout_;

  // Pointer to the first allocated audio buffer. This object owns it.
  WAVEHDR* buffer_;

  // Lock used to prevent stopping the hardware callback thread while it is
  // pending for data or feeding it to audio driver, because doing that causes
  // the deadlock. Main thread gets that lock before stopping the playback.
  // Callback tries to acquire that lock before entering critical code. If
  // acquire fails then main thread is stopping the playback, callback should
  // immediately return.
  // Use Windows-specific lock, not Chrome one, because there is limited set of
  // functions callback can use.
  CRITICAL_SECTION lock_;

  DISALLOW_COPY_AND_ASSIGN(PCMWaveOutAudioOutputStream);
};

#endif  // MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
