// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_OUTPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_OUTPUT_MAC_H_

#include <AudioUnit/AudioUnit.h>

#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class AudioManagerMac;

// Implementation of AudioOuputStream for Mac OS X using the
// default output Audio Unit present in OS 10.4 and later.
// The default output Audio Unit is for low-latency audio I/O.
class AUAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AUAudioOutputStream(AudioManagerMac* manager,
                      AudioParameters params);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~AUAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open();
  virtual void Close();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);

  static double HardwareSampleRate();

 private:
  // DefaultOutputUnit callback.
  static OSStatus InputProc(void* user_data,
                            AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 number_of_frames,
                            AudioBufferList* io_data);

  OSStatus Render(UInt32 number_of_frames, AudioBufferList* io_data);

  // Sets up the stream format for the default output Audio Unit.
  bool Configure();

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* manager_;

  size_t number_of_frames_;

  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;

  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;

  // The default output Audio Unit which talks to the audio hardware.
  AudioUnit output_unit_;

  // Volume level from 0 to 1.
  float volume_;

  DISALLOW_COPY_AND_ASSIGN(AUAudioOutputStream);
};

#endif  // MEDIA_AUDIO_MAC_AUDIO_LOW_LATENCY_OUTPUT_MAC_H_
