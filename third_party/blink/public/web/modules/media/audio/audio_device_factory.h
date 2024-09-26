// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_AUDIO_DEVICE_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_AUDIO_DEVICE_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/audio/audio_source_parameters.h"
#include "media/base/audio_latency.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/web_common.h"

namespace media {
class AudioRendererSink;
class SwitchableAudioRendererSink;
class AudioCapturerSource;
}  // namespace media

namespace blink {

class AudioRendererMixerManager;
class AudioRendererSinkCache;
class WebLocalFrame;

// A factory for creating AudioRendererSinks and AudioCapturerSources. There is
// a global factory function that can be installed for the purposes of testing
// to provide specialized implementations.
// Public methods can be called only on the main (renderer) thread.
// TODO(crbug.com/1255249): Rename the class and probably split it into
// AudioRendererSinkFactory and AudioCapturerSourceFactory.
class BLINK_MODULES_EXPORT AudioDeviceFactory {
 public:
  // Returns an instance of this class for the current process.
  static AudioDeviceFactory* GetInstance();

  explicit AudioDeviceFactory(bool override_default = true);

  AudioDeviceFactory(const AudioDeviceFactory&) = delete;
  AudioDeviceFactory& operator=(const AudioDeviceFactory&) = delete;

  // Maps the source type to the audio latency it requires.
  static media::AudioLatency::LatencyType GetSourceLatencyType(
      WebAudioDeviceSourceType source);

  // Creates an AudioRendererSink bound to an AudioOutputDevice.
  // Basing on |source_type| and build configuration, audio played out through
  // the sink goes to AOD directly or can be mixed with other audio before that.
  // TODO(olka): merge it with NewRestartableOutputDevice() as soon as
  // AudioOutputDevice is fixed to be restartable.
  virtual scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      WebAudioDeviceSourceType source_type,
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params);

  // Creates a SwitchableAudioRendererSink bound to an AudioOutputDevice
  // Basing on |source_type| and build configuration, audio played out through
  // the sink goes to AOD directly or can be mixed with other audio before that.
  virtual scoped_refptr<media::SwitchableAudioRendererSink>
  NewSwitchableAudioRendererSink(WebAudioDeviceSourceType source_type,
                                 const LocalFrameToken& frame_token,
                                 const media::AudioSinkParameters& params);

  // A helper to get device info in the absence of AudioOutputDevice.
  // |device_id| identifies which device we are getting info from.
  // |frame_token| is used to created a temporary sink to retrieve the info.
  virtual media::OutputDeviceInfo GetOutputDeviceInfo(
      const LocalFrameToken& frame_token,
      const std::string& device_id);

  // Creates an AudioCapturerSource using the currently registered factory.
  // |frame_token| refers to the RenderFrame containing the entity
  // consuming the audio.
  virtual scoped_refptr<media::AudioCapturerSource> NewAudioCapturerSource(
      WebLocalFrame* web_frame,
      const media::AudioSourceParameters& params);

 protected:
  virtual ~AudioDeviceFactory();

  // Creates a sink for a stream that can be mixed with other streams.
  scoped_refptr<media::SwitchableAudioRendererSink> NewMixableSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params);

  // Creates a sink for AudioRendererMixer. |frame_token| refers to the
  // RenderFrame containing the entity producing the audio. Note: These sinks do
  // not support the blocking GetOutputDeviceInfo() API and instead clients are
  // required to use the GetOutputDeviceInfoAsync() API. As such they are
  // configured with no authorization timeout value.
  virtual scoped_refptr<media::AudioRendererSink> NewAudioRendererMixerSink(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params);

  // Creates a final sink in the rendering pipeline, which represents the actual
  // output device. |auth_timeout| is the authorization timeout allowed for the
  // underlying AudioOutputDevice instance; a timeout of zero means no timeout.
  scoped_refptr<media::AudioRendererSink> NewFinalAudioRendererSink(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout);

 private:
  std::unique_ptr<AudioRendererMixerManager> mixer_manager_;
  std::unique_ptr<AudioRendererSinkCache> sink_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_AUDIO_DEVICE_FACTORY_H_
