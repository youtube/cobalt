// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_SYNCHRONIZED_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_SYNCHRONIZED_MAC_H_

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_fifo.h"

namespace media {

class AudioManagerMac;

// AudioSynchronizedStream allows arbitrary combinations of input and output
// devices running off different clocks and using different drivers, with
// potentially differing sample-rates.  It implements AudioOutputStream
// and shuttles its synchronized I/O data using AudioSourceCallback.
//
// It is required to first acquire the native sample rate of the selected
// output device and then use the same rate when creating this object.
//
// ............................................................................
// Theory of Operation:
//                                       .
//      INPUT THREAD                     .             OUTPUT THREAD
//   +-----------------+     +------+    .
//   | Input AudioUnit | --> |      |    .
//   +-----------------+     |      |    .
//                           | FIFO |    .
//                           |      |        +-----------+
//                           |      | -----> | Varispeed |
//                           |      |        +-----------+
//                           +------+    .         |
//                                       .         |              +-----------+
//                                       .    OnMoreIOData() -->  | Output AU |
//                                       .                        +-----------+
//
// The input AudioUnit's InputProc is called on one thread which feeds the
// FIFO.  The output AudioUnit's OutputProc is called on a second thread
// which pulls on the varispeed to get the current input data.  The varispeed
// handles mismatches between input and output sample-rate and also clock drift
// between the input and output drivers.  The varispeed consumes its data from
// the FIFO and adjusts its rate dynamically according to the amount
// of data buffered in the FIFO.  If the FIFO starts getting too much data
// buffered then the varispeed will speed up slightly to compensate
// and similarly if the FIFO doesn't have enough data buffered then the
// varispeed will slow down slightly.
//
// Finally, once the input data is available then OnMoreIOData() is called
// which is given this input, and renders the output which is finally sent
// to the Output AudioUnit.
class AudioSynchronizedStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AudioSynchronizedStream(AudioManagerMac* manager,
                          const AudioParameters& params,
                          AudioDeviceID input_id,
                          AudioDeviceID output_id);

  virtual ~AudioSynchronizedStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() override;
  virtual void Close() override;
  virtual void Start(AudioSourceCallback* callback) override;
  virtual void Stop() override;

  virtual void SetVolume(double volume) override;
  virtual void GetVolume(double* volume) override;

  OSStatus SetInputDeviceAsCurrent(AudioDeviceID input_id);
  OSStatus SetOutputDeviceAsCurrent(AudioDeviceID output_id);
  AudioDeviceID GetInputDeviceID() { return input_info_.id_;  }
  AudioDeviceID GetOutputDeviceID() { return output_info_.id_; }

  bool IsRunning();

 private:
  // Initialization.
  OSStatus CreateAudioUnits();
  OSStatus SetupInput(AudioDeviceID input_id);
  OSStatus EnableIO();
  OSStatus SetupOutput(AudioDeviceID output_id);
  OSStatus SetupCallbacks();
  OSStatus SetupStreamFormats();
  void AllocateInputData();

  // Handlers for the AudioUnit callbacks.
  OSStatus HandleInputCallback(AudioUnitRenderActionFlags* io_action_flags,
                               const AudioTimeStamp* time_stamp,
                               UInt32 bus_number,
                               UInt32 number_of_frames,
                               AudioBufferList* io_data);

  OSStatus HandleVarispeedCallback(AudioUnitRenderActionFlags* io_action_flags,
                                   const AudioTimeStamp* time_stamp,
                                   UInt32 bus_number,
                                   UInt32 number_of_frames,
                                   AudioBufferList* io_data);

  OSStatus HandleOutputCallback(AudioUnitRenderActionFlags* io_action_flags,
                                const AudioTimeStamp* time_stamp,
                                UInt32 bus_number,
                                UInt32 number_of_frames,
                                AudioBufferList* io_data);

  // AudioUnit callbacks.
  static OSStatus InputProc(void* user_data,
                            AudioUnitRenderActionFlags* io_action_flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 number_of_frames,
                            AudioBufferList* io_data);

  static OSStatus VarispeedProc(void* user_data,
                                AudioUnitRenderActionFlags* io_action_flags,
                                const AudioTimeStamp* time_stamp,
                                UInt32 bus_number,
                                UInt32 number_of_frames,
                                AudioBufferList* io_data);

  static OSStatus OutputProc(void* user_data,
                             AudioUnitRenderActionFlags* io_action_flags,
                             const AudioTimeStamp* time_stamp,
                             UInt32 bus_number,
                             UInt32 number_of_frames,
                             AudioBufferList*  io_data);

  // Our creator.
  AudioManagerMac* manager_;

  // Client parameters.
  AudioParameters params_;

  double input_sample_rate_;
  double output_sample_rate_;

  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;

  // Values used in Open().
  AudioDeviceID input_id_;
  AudioDeviceID output_id_;

  // The input AudioUnit renders its data here.
  AudioBufferList* input_buffer_list_;

  // Holds the actual data for |input_buffer_list_|.
  scoped_ptr<AudioBus> input_bus_;

  // Used to overlay AudioBufferLists.
  scoped_ptr<AudioBus> wrapper_bus_;

  class AudioDeviceInfo {
   public:
    AudioDeviceInfo()
        : id_(kAudioDeviceUnknown),
          is_input_(false),
          buffer_size_frames_(0) {}
    void Initialize(AudioDeviceID inID, bool isInput);
    bool IsInitialized() const { return id_ != kAudioDeviceUnknown; }

    AudioDeviceID id_;
    bool is_input_;
    UInt32 buffer_size_frames_;
  };

  AudioDeviceInfo input_info_;
  AudioDeviceInfo output_info_;

  // Used for input to output buffering.
  AudioFifo fifo_;

  // The optimal number of frames we'd like to keep in the FIFO at all times.
  int target_fifo_frames_;

  // A running average of the measured delta between actual number of frames
  // in the FIFO versus |target_fifo_frames_|.
  double average_delta_;

  // A varispeed rate scalar which is calculated based on FIFO drift.
  double fifo_rate_compensation_;

  // AudioUnits.
  AudioUnit input_unit_;
  AudioUnit varispeed_unit_;
  AudioUnit output_unit_;

  double first_input_time_;

  bool is_running_;
  int hardware_buffer_size_;
  int channels_;

  DISALLOW_COPY_AND_ASSIGN(AudioSynchronizedStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_SYNCHRONIZED_MAC_H_
