// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_OUTPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_OUTPUT_MAC_H_

#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioQueue.h>

#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class AudioManagerMac;

// Implementation of AudioOuputStream for Mac OS X using the audio queue service
// present in OS 10.5 and later. Audioqueue is the successor to the SoundManager
// services but it is supported in 64 bits.
class PCMQueueOutAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  PCMQueueOutAudioOutputStream(AudioManagerMac* manager,
                               AudioParameters params);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~PCMQueueOutAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open(uint32 packet_size);
  virtual void Close();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);

 private:
  // The audio is double buffered.
  static const uint32 kNumBuffers = 2;

  // The OS calls back here when an audio buffer has been processed.
  static void RenderCallback(void* p_this, AudioQueueRef queue,
                             AudioQueueBufferRef buffer);
  // Called when an error occurs.
  void HandleError(OSStatus err);

  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;
  // Handle to the OS audio queue object.
  AudioQueueRef audio_queue_;
  // Array of pointers to the OS managed audio buffers.
  AudioQueueBufferRef buffer_[kNumBuffers];
  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;
  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* manager_;
  // Number of bytes for making a silence buffer.
  int silence_bytes_;
  // Volume level from 0 to 1.
  float volume_;
  // Number of bytes yet to be played in audio buffer.
  uint32 pending_bytes_;

  DISALLOW_COPY_AND_ASSIGN(PCMQueueOutAudioOutputStream);
};

#endif  // MEDIA_AUDIO_MAC_AUDIO_OUTPUT_MAC_H_
