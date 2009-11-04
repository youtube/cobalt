// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates an output stream based on the ALSA PCM interface.
//
// On device write failure, the stream will move itself to an invalid state.
// No more data will be pulled from the data source, or written to the device.
// All calls to public API functions will either no-op themselves, or return an
// error if possible.  Specifically, If the stream is in an error state, Open()
// will return false, and Start() will call OnError() immediately on the
// provided callback.
//
// TODO(ajwong): The OnClose() and OnError() calling needing fixing.
//
// If the stream is successfully opened, Close() must be called before the
// stream is deleted as Close() is responsible for ensuring resource cleanup
// occurs.
//
// This object's thread-safety is a little tricky.  This object's public API
// can only be called from the thread that created the object.  Calling the
// public APIs in any method that may cause concurrent execution will result in
// a race condition.  When modifying the code in this class, please read the
// threading assumptions at the top of the implementation file to avoid
// introducing race conditions between tasks posted to the internal
// message_loop, and the thread calling the public APIs.

#ifndef MEDIA_AUDIO_LINUX_ALSA_OUTPUT_H_
#define MEDIA_AUDIO_LINUX_ALSA_OUTPUT_H_

#include <alsa/asoundlib.h>

#include <string>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "media/audio/audio_output.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class AlsaWrapper;
class AudioManagerLinux;

class AlsaPcmOutputStream :
    public AudioOutputStream,
    public base::RefCountedThreadSafe<AlsaPcmOutputStream> {
 public:
  // String for the generic "default" ALSA device that has the highest
  // compatibility and chance of working.
  static const char kDefaultDevice[];

  // Pass this to the AlsaPcmOutputStream if you want to attempt auto-selection
  // of the audio device.
  static const char kAutoSelectDevice[];

  // Prefix for device names to enable ALSA library resampling.
  static const char kPlugPrefix[];

  // The minimum latency that is accepted by the device.
  static const int kMinLatencyMicros;

  // Create a PCM Output stream for the ALSA device identified by
  // |device_name|.  The AlsaPcmOutputStream uses |wrapper| to communicate with
  // the alsa libraries, allowing for dependency injection during testing.  All
  // requesting of data, and writing to the alsa device will be done on
  // |message_loop|.
  //
  // If unsure of what to use for |device_name|, use |kAutoSelectDevice|.
  AlsaPcmOutputStream(const std::string& device_name,
                      AudioManager::Format format,
                      int channels,
                      int sample_rate,
                      int bits_per_sample,
                      AlsaWrapper* wrapper,
                      AudioManagerLinux* manager,
                      MessageLoop* message_loop);
  virtual ~AlsaPcmOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open(size_t packet_size);
  virtual void Close();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);

 private:
  friend class AlsaPcmOutputStreamTest;
  FRIEND_TEST(AlsaPcmOutputStreamTest, AutoSelectDevice_DeviceSelect);
  FRIEND_TEST(AlsaPcmOutputStreamTest, AutoSelectDevice_FallbackDevices);
  FRIEND_TEST(AlsaPcmOutputStreamTest, AutoSelectDevice_HintFail);
  FRIEND_TEST(AlsaPcmOutputStreamTest, BufferPacket);
  FRIEND_TEST(AlsaPcmOutputStreamTest, BufferPacket_StopStream);
  FRIEND_TEST(AlsaPcmOutputStreamTest, BufferPacket_UnfinishedPacket);
  FRIEND_TEST(AlsaPcmOutputStreamTest, ConstructedState);
  FRIEND_TEST(AlsaPcmOutputStreamTest, LatencyFloor);
  FRIEND_TEST(AlsaPcmOutputStreamTest, OpenClose);
  FRIEND_TEST(AlsaPcmOutputStreamTest, PcmOpenFailed);
  FRIEND_TEST(AlsaPcmOutputStreamTest, PcmSetParamsFailed);
  FRIEND_TEST(AlsaPcmOutputStreamTest, ScheduleNextWrite);
  FRIEND_TEST(AlsaPcmOutputStreamTest, ScheduleNextWrite_StopStream);
  FRIEND_TEST(AlsaPcmOutputStreamTest, StartStop);
  FRIEND_TEST(AlsaPcmOutputStreamTest, WritePacket_FinishedPacket);
  FRIEND_TEST(AlsaPcmOutputStreamTest, WritePacket_NormalPacket);
  FRIEND_TEST(AlsaPcmOutputStreamTest, WritePacket_StopStream);
  FRIEND_TEST(AlsaPcmOutputStreamTest, WritePacket_WriteFails);

  // In-memory buffer to hold sound samples before pushing to the sound device.
  // TODO(ajwong): There are now about 3 buffer/packet implementations. Factor
  // them out.
  struct Packet {
    explicit Packet(int new_capacity)
        : capacity(new_capacity),
          size(0),
          used(0),
          buffer(new char[capacity]) {
    }
    size_t capacity;
    size_t size;
    size_t used;
    scoped_array<char> buffer;
  };

  // Flags indicating the state of the stream.
  enum InternalState {
    kInError = 0,
    kCreated,
    kIsOpened,
    kIsPlaying,
    kIsStopped,
    kIsClosed
  };
  friend std::ostream& ::operator<<(std::ostream& os, InternalState);

  // Various tasks that complete actions started in the public API.
  void OpenTask(size_t packet_size);
  void StartTask();
  void CloseTask();

  // Functions to get another packet from the data source and write it into the
  // ALSA device.
  void BufferPacket(Packet* packet);
  void WritePacket(Packet* packet);
  void WriteTask();
  void ScheduleNextWrite(Packet* current_packet);

  // Utility functions for talking with the ALSA API.
  static snd_pcm_sframes_t FramesInPacket(const Packet& packet,
                                          int bytes_per_frame);
  static int64 FramesToMicros(int frames, int sample_rate);
  static int64 FramesToMillis(int frames, int sample_rate);
  std::string FindDeviceForChannels(int channels);
  snd_pcm_t* OpenDevice(const std::string& device_name,
                        int channels,
                        unsigned int latency);
  bool CloseDevice(snd_pcm_t* handle);
  snd_pcm_sframes_t GetAvailableFrames();

  // Attempts to find the best matching linux audio device for the given number
  // of channels.  This function will set |device_name_| and |should_downmix_|.
  snd_pcm_t* AutoSelectDevice(unsigned int latency);

  // Thread-asserting accessors for member variables.
  AudioManagerLinux* manager();

  // Struct holding all mutable the data that must be shared by the
  // message_loop() and the thread that created the object.
  class SharedData {
   public:
    explicit SharedData(MessageLoop* state_transition_loop);

    // Functions to safeguard state transitions and ensure that transitions are
    // only allowed occuring on the thread that created the object.  All
    // changes to the object state should go through these functions.
    bool CanTransitionTo(InternalState to);
    bool CanTransitionTo_Locked(InternalState to);
    InternalState TransitionTo(InternalState to);
    InternalState state();

    float volume();
    void set_volume(float v);

    // API for Proxying calls to the AudioSourceCallback provided during
    // Start().  These APIs are threadsafe.
    //
    // TODO(ajwong): This is necessary because the ownership semantics for the
    // |source_callback_| object are incorrect in AudioRenderHost. The callback
    // is passed into the output stream, but ownership is not transfered which
    // requires a synchronization on access of the |source_callback_| to avoid
    // using a deleted callback.
    size_t OnMoreData(AudioOutputStream* stream, void* dest,
                      size_t max_size, int pending_bytes);
    void OnClose(AudioOutputStream* stream);
    void OnError(AudioOutputStream* stream, int code);

    // Changes the AudioSourceCallback to proxy calls to.  Pass in NULL to
    // release ownership of the currently registered callback.
    void set_source_callback(AudioSourceCallback* callback);

   private:
    Lock lock_;

    InternalState state_;
    float volume_;  // Volume level from 0.0 to 1.0.

    AudioSourceCallback* source_callback_;

    MessageLoop* const state_transition_loop_;

    DISALLOW_COPY_AND_ASSIGN(SharedData);
  } shared_data_;

  // Configuration constants from the constructor.  Referenceable by all threads
  // since they are constants.
  const std::string requested_device_name_;
  const snd_pcm_format_t pcm_format_;
  const int channels_;
  const int sample_rate_;
  const int bytes_per_sample_;
  const int bytes_per_frame_;

  // Device configuration data. Populated after OpenTask() completes.
  std::string device_name_;
  bool should_downmix_;
  int latency_micros_;
  int micros_per_packet_;
  int bytes_per_output_frame_;

  // Flag indicating the code should stop reading from the data source or
  // writing to the ALSA device.  This is set because the device has entered
  // an unrecoverable error state, or the ClosedTask() has executed.
  bool stop_stream_;

  // Wrapper class to invoke all the ALSA functions.
  AlsaWrapper* wrapper_;

  // Audio manager that created us.  Used to report that we've been closed.
  // This should only be used on the |client_thread_loop_|.  Access via
  // the manager() function.
  AudioManagerLinux* manager_;

  // Handle to the actual PCM playback device.
  snd_pcm_t* playback_handle_;

  scoped_ptr<Packet> packet_;
  int frames_per_packet_;

  // Used to check which message loop is allowed to call the public APIs.
  MessageLoop* client_thread_loop_;

  // The message loop responsible for querying the data source, and writing to
  // the output device.
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AlsaPcmOutputStream);
};

#endif  // MEDIA_AUDIO_LINUX_ALSA_OUTPUT_H_
