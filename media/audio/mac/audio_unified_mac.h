// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_UNIFIED_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_UNIFIED_MAC_H_

#include <CoreAudio/CoreAudio.h>

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerMac;

// Implementation of AudioOutputStream for Mac OS X using the
// CoreAudio AudioHardware API suitable for low-latency unified audio I/O
// when using devices which support *both* input and output
// in the same driver.  This is the case with professional
// USB and Firewire devices.
//
// Please note that it's required to first get the native sample-rate of the
// default output device and use that sample-rate when creating this object.
class AudioHardwareUnifiedStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AudioHardwareUnifiedStream(AudioManagerMac* manager,
                             const AudioParameters& params);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~AudioHardwareUnifiedStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() override;
  virtual void Close() override;
  virtual void Start(AudioSourceCallback* callback) override;
  virtual void Stop() override;
  virtual void SetVolume(double volume) override;
  virtual void GetVolume(double* volume) override;

  int input_channels() const { return input_channels_; }
  int output_channels() const { return output_channels_; }

 private:
  OSStatus Render(AudioDeviceID device,
                  const AudioTimeStamp* now,
                  const AudioBufferList* input_data,
                  const AudioTimeStamp* input_time,
                  AudioBufferList* output_data,
                  const AudioTimeStamp* output_time);

  static OSStatus RenderProc(AudioDeviceID device,
                             const AudioTimeStamp* now,
                             const AudioBufferList* input_data,
                             const AudioTimeStamp* input_time,
                             AudioBufferList* output_data,
                             const AudioTimeStamp* output_time,
                             void* user_data);

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* manager_;

  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;

  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;

  // Hardware buffer size.
  int number_of_frames_;

  // Number of audio channels provided to the client via OnMoreIOData().
  int client_input_channels_;

  // Volume level from 0 to 1.
  float volume_;

  // Number of input and output channels queried from the hardware.
  int input_channels_;
  int output_channels_;
  int input_channels_per_frame_;
  int output_channels_per_frame_;

  AudioDeviceIOProcID io_proc_id_;
  AudioDeviceID device_;
  bool is_playing_;

  // Intermediate buffers used with call to OnMoreIOData().
  scoped_ptr<AudioBus> input_bus_;
  scoped_ptr<AudioBus> output_bus_;

  DISALLOW_COPY_AND_ASSIGN(AudioHardwareUnifiedStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_UNIFIED_MAC_H_
