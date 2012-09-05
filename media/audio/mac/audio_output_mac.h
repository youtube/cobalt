// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_OUTPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_OUTPUT_MAC_H_

#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioQueue.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerMac;

// Implementation of AudioOuputStream for Mac OS X using the audio queue service
// present in OS 10.5 and later. Audioqueue is the successor to the SoundManager
// services but it is supported in 64 bits.
class PCMQueueOutAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  PCMQueueOutAudioOutputStream(AudioManagerMac* manager,
                               const AudioParameters& params);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~PCMQueueOutAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  // The audio is double buffered.
  static const uint32 kNumBuffers = 2;
  static const int kEmptyChannel = -1;

  // Reorder PCM from source layout to device layout found in Core Audio.
  template<class Format>
  void SwizzleLayout(Format* b, uint32 filled);
  // Check and move channels if surround sound layout needs adjusted.
  bool CheckForAdjustedLayout(Channels input_channel, Channels output_channel);

  // The OS calls back here when an audio buffer has been processed.
  static void RenderCallback(void* p_this, AudioQueueRef queue,
                             AudioQueueBufferRef buffer);
  // Called when an error occurs.
  void HandleError(OSStatus err);

  // Atomic operations for setting/getting the source callback.
  void SetSource(AudioSourceCallback* source);
  AudioSourceCallback* GetSource();

  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;
  // Handle to the OS audio queue object.
  AudioQueueRef audio_queue_;
  // Array of pointers to the OS managed audio buffers.
  AudioQueueBufferRef buffer_[kNumBuffers];
  // Mutex for the |source_| to implment atomic set and get.
  // It is important to NOT wait on any other locks while this is held.
  base::Lock source_lock_;
  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;
  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* manager_;
  // Packet size in bytes.
  uint32 packet_size_;
  // Number of bytes for making a silence buffer.
  int silence_bytes_;
  // Volume level from 0 to 1.
  float volume_;
  // Number of bytes yet to be played in audio buffer.
  uint32 pending_bytes_;
  // Number of channels in the source audio.
  int num_source_channels_;
  // Source's channel layout for surround sound channels.
  ChannelLayout source_layout_;
  // Device's channel layout.
  int core_channel_orderings_[CHANNELS_MAX];
  // An array for remapping source to device channel layouts during a swizzle.
  int channel_remap_[CHANNELS_MAX];
  // Number of channels in device layout.
  int num_core_channels_;
  // A flag to determine if swizzle is needed from source to device layouts.
  bool should_swizzle_;
  // A flag to determine if downmix is needed from source to device layouts.
  bool should_down_mix_;

  // Event used for synchronization when stopping the stream.
  // Callback sets it after stream is stopped.
  base::WaitableEvent stopped_event_;
  // When stopping we keep track of number of buffers in flight and
  // signal "stop completed" from the last buffer's callback.
  int num_buffers_left_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(PCMQueueOutAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_OUTPUT_MAC_H_
