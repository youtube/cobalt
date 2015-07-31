// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes:
//
// - It is recommended to first acquire the native sample rate of the default
//   output device and then use the same rate when creating this object.
//   Use AUAudioOutputStream::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
// - The latency consists of two parts:
//   1) Hardware latency, which includes Audio Unit latency, audio device
//      latency;
//   2) The delay between the moment getting the callback and the scheduled time
//      stamp that tells when the data is going to be played out.
//
#ifndef MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_OUTPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_OUTPUT_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "base/compiler_specific.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerMac;

// Implementation of AudioOuputStream for Mac OS X using the
// default output Audio Unit present in OS 10.4 and later.
// The default output Audio Unit is for low-latency audio I/O.
class AUAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AUAudioOutputStream(AudioManagerMac* manager,
                      const AudioParameters& params);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~AUAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  static int HardwareSampleRate();

 private:
  // DefaultOutputUnit callback.
  static OSStatus InputProc(void* user_data,
                            AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 number_of_frames,
                            AudioBufferList* io_data);

  OSStatus Render(UInt32 number_of_frames, AudioBufferList* io_data,
                  const AudioTimeStamp* output_time_stamp);

  // Sets up the stream format for the default output Audio Unit.
  bool Configure();

  // Gets the fixed playout device hardware latency and stores it. Returns 0
  // if not available.
  double GetHardwareLatency();

  // Gets the current playout latency value.
  double GetPlayoutLatency(const AudioTimeStamp* output_time_stamp);

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* manager_;

  size_t number_of_frames_;

  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;

  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;

  // The default output Audio Unit which talks to the audio hardware.
  AudioUnit output_unit_;

  // The UID refers to the current output audio device.
  AudioDeviceID output_device_id_;

  // Volume level from 0 to 1.
  float volume_;

  // Fixed playout hardware latency in frames.
  double hardware_latency_frames_;

  // The flag used to stop the streaming.
  bool stopped_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(AUAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_OUTPUT_MAC_H_
