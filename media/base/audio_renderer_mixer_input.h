// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THREAD SAFETY
//
// This class is generally not thread safe. Callers should ensure thread safety.
// For instance, the |sink_lock_| in WebAudioSourceProvider synchronizes access
// to this object across the main thread (for WebAudio APIs) and the
// media thread (for HTMLMediaElement APIs).
//
// The one exception is protection for |volume_| via |volume_lock_|. This lock
// prevents races between SetVolume() (called on any thread) and ProvideInput
// (called on audio device thread). See http://crbug.com/588992.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

class AudioRendererMixerPool;
class AudioRendererMixer;

class MEDIA_EXPORT AudioRendererMixerInput
    : public SwitchableAudioRendererSink,
      public AudioConverter::InputCallback {
 public:
  AudioRendererMixerInput(AudioRendererMixerPool* mixer_pool,
                          const base::UnguessableToken& owner_token,
                          const std::string& device_id,
                          AudioLatency::LatencyType latency);

  AudioRendererMixerInput(const AudioRendererMixerInput&) = delete;
  AudioRendererMixerInput& operator=(const AudioRendererMixerInput&) = delete;

  // SwitchableAudioRendererSink implementation.
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  void Flush() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;

  bool IsOptimizedForHardwareParameters() override;
  void Initialize(const AudioParameters& params,
                  AudioRendererSink::RenderCallback* renderer) override;
  void SwitchOutputDevice(const std::string& device_id,
                          OutputDeviceStatusCB callback) override;
  // This is expected to be called on the audio rendering thread. The caller
  // must ensure that this input has been added to a mixer before calling the
  // function, and that it is not removed from the mixer before this function
  // returns.
  bool CurrentThreadIsRenderingThread() override;

  // Called by AudioRendererMixer when an error occurs.
  void OnRenderError();

 protected:
  ~AudioRendererMixerInput() override;

 private:
  friend class AudioRendererMixerInputTest;

  // Pool to obtain mixers from / return them to.
  const raw_ptr<AudioRendererMixerPool> mixer_pool_;

  // Protect |volume_|, accessed by separate threads in ProvideInput() and
  // SetVolume().
  base::Lock volume_lock_;

  bool started_ = false;
  bool playing_ = false;
  double volume_ GUARDED_BY(volume_lock_) = 1.0;

  scoped_refptr<AudioRendererSink> sink_;
  absl::optional<OutputDeviceInfo> device_info_;

  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  void OnDeviceInfoReceived(OutputDeviceInfoCB info_cb,
                            OutputDeviceInfo device_info);

  // Method to help handle device changes. Must be static to ensure we can still
  // execute the |switch_cb| even if the pipeline is destructed. Restarts (if
  // necessary) Start() and Play() state with a new |sink| and |device_info|.
  //
  // |switch_cb| is the callback given to the SwitchOutputDevice() call.
  // |sink| is a fresh sink which should be used if device info is good.
  // |device_info| is the OutputDeviceInfo for |sink| after
  // GetOutputDeviceInfoAsync() completes.
  void OnDeviceSwitchReady(OutputDeviceStatusCB switch_cb,
                           scoped_refptr<AudioRendererSink> sink,
                           OutputDeviceInfo device_info);

  // AudioParameters received during Initialize().
  AudioParameters params_;

  // Linearly fades in the input volume during the first ProvideInput() calls,
  // avoiding audible pops.
  int total_fade_in_frames_;
  int remaining_fade_in_frames_ = 0;

  const base::UnguessableToken owner_token_;
  std::string device_id_;  // ID of hardware device to use
  const AudioLatency::LatencyType latency_;

  // AudioRendererMixer obtained from mixer pool during Initialize(),
  // guaranteed to live (at least) until it is returned to the pool.
  raw_ptr<AudioRendererMixer, DanglingUntriaged> mixer_ = nullptr;

  // Source of audio data which is provided to the mixer.
  raw_ptr<AudioRendererSink::RenderCallback, DanglingUntriaged> callback_ =
      nullptr;

  // SwitchOutputDevice() and GetOutputDeviceInfoAsync() must be mutually
  // exclusive when executing; these flags indicate whether one or the other is
  // in progress. Each method will use the other method's to defer its action.
  bool godia_in_progress_ = false;
  bool switch_output_device_in_progress_ = false;

  // Set by GetOutputDeviceInfoAsync() if a SwitchOutputDevice() call is in
  // progress. GetOutputDeviceInfoAsync() will be invoked again with this value
  // once OnDeviceSwitchReady() from the SwitchOutputDevice() call completes.
  OutputDeviceInfoCB pending_device_info_cb_;

  // Set by SwitchOutputDevice() if a GetOutputDeviceInfoAsync() call is in
  // progress. SwitchOutputDevice() will be invoked again with these values once
  // the OnDeviceInfoReceived() from the GODIA() call completes.
  std::string pending_device_id_;
  OutputDeviceStatusCB pending_switch_cb_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
