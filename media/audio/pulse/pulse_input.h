// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_INPUT_H_
#define MEDIA_AUDIO_PULSE_PULSE_INPUT_H_

#include <pulse/pulseaudio.h>
#include <stddef.h>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerPulse;

class PulseAudioInputStream : public AgcAudioStream<AudioInputStream> {
 public:
  PulseAudioInputStream(AudioManagerPulse* audio_manager,
                        const std::string& device_name,
                        const AudioParameters& params,
                        pa_threaded_mainloop* mainloop,
                        pa_context* context,
                        AudioManager::LogCallback log_callback);

  PulseAudioInputStream(const PulseAudioInputStream&) = delete;
  PulseAudioInputStream& operator=(const PulseAudioInputStream&) = delete;

  ~PulseAudioInputStream() override;

  // Implementation of AudioInputStream.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  // Helper method used for sending native logs to the registered client.
  void SendLogMessage(const char* format, ...) PRINTF_FORMAT(2, 3);

  // PulseAudio Callbacks.
  static void ReadCallback(pa_stream* handle, size_t length, void* user_data);
  static void StreamNotifyCallback(pa_stream* stream, void* user_data);
  static void VolumeCallback(pa_context* context, const pa_source_info* info,
                             int error, void* user_data);
  static void MuteCallback(pa_context* context,
                           const pa_source_info* info,
                           int error,
                           void* user_data);

  // Helper for the ReadCallback.
  void ReadData();

  // Utility method used by GetVolume() and IsMuted().
  bool GetSourceInformation(pa_source_info_cb_t callback);

  AudioManagerPulse* audio_manager_;
  AudioInputCallback* callback_;
  std::string device_name_;
  AudioParameters params_;
  int channels_;
  double volume_;
  bool stream_started_;

  // Set to true in IsMuted() if user has muted the selected microphone in the
  // sound settings UI.
  bool muted_;

  // Holds the data from the OS.
  AudioBlockFifo fifo_;

  // PulseAudio API structs.
  pa_threaded_mainloop* pa_mainloop_; // Weak.

  pa_context* pa_context_;  // Weak.

  // Callback to send log messages to registered clients.
  AudioManager::LogCallback log_callback_;

  pa_stream* handle_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_INPUT_H_
