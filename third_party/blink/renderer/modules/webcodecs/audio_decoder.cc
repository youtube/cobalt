// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

#include "base/metrics/histogram_functions.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/waiting.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

#include <memory>
#include <vector>

namespace blink {

AudioDecoderConfig* CopyConfig(const AudioDecoderConfig& config) {
  AudioDecoderConfig* copy = AudioDecoderConfig::Create();
  copy->setCodec(config.codec());
  copy->setSampleRate(config.sampleRate());
  copy->setNumberOfChannels(config.numberOfChannels());
  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.empty()) {
      DOMArrayBuffer* buffer_copy =
          DOMArrayBuffer::Create(desc_wrapper.data(), desc_wrapper.size());
      copy->setDescription(
          MakeGarbageCollected<AllowSharedBufferSource>(buffer_copy));
    }
  }
  return copy;
}

// static
std::unique_ptr<AudioDecoderTraits::MediaDecoderType>
AudioDecoderTraits::CreateDecoder(
    ExecutionContext& execution_context,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log) {
  return std::make_unique<AudioDecoderBroker>(media_log, execution_context);
}

// static
void AudioDecoderTraits::UpdateDecoderLog(const MediaDecoderType& decoder,
                                          const MediaConfigType& media_config,
                                          media::MediaLog* media_log) {
  media_log->SetProperty<media::MediaLogProperty::kAudioDecoderName>(
      decoder.GetDecoderType());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformAudioDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kAudioTracks>(
      std::vector<MediaConfigType>{media_config});
  MEDIA_LOG(INFO, media_log)
      << "Initialized AudioDecoder: " << media_config.AsHumanReadableString();
  base::UmaHistogramEnumeration("Blink.WebCodecs.AudioDecoder.Codec",
                                media_config.codec());
}

// static
void AudioDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    bool /*low_delay*/,
    const MediaConfigType& media_config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  decoder.Initialize(media_config, nullptr /* cdm_context */,
                     std::move(init_cb), output_cb, media::WaitingCB());
}

// static
int AudioDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return 1;
}

// static
const char* AudioDecoderTraits::GetName() {
  return "AudioDecoder";
}

// static
AudioDecoder* AudioDecoder::Create(ScriptState* script_state,
                                   const AudioDecoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<AudioDecoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

// static
ScriptPromise AudioDecoder::isConfigSupported(ScriptState* script_state,
                                              const AudioDecoderConfig* config,
                                              ExceptionState& exception_state) {
  String js_error_message;
  absl::optional<media::AudioType> audio_type =
      IsValidAudioDecoderConfig(*config, &js_error_message);

  if (!audio_type) {
    exception_state.ThrowTypeError(js_error_message);
    return ScriptPromise();
  }

  AudioDecoderSupport* support = AudioDecoderSupport::Create();
  support->setSupported(media::IsSupportedAudioType(*audio_type));
  support->setConfig(CopyConfig(*config));

  return ScriptPromise::Cast(
      script_state, ToV8Traits<AudioDecoderSupport>::ToV8(script_state, support)
                        .ToLocalChecked());
}

// static
absl::optional<media::AudioType> AudioDecoder::IsValidAudioDecoderConfig(
    const AudioDecoderConfig& config,
    String* js_error_message) {
  media::AudioType audio_type;

  // Match codec strings from the codec registry:
  // https://www.w3.org/TR/webcodecs-codec-registry/#audio-codec-registry
  if (config.codec() == "ulaw") {
    audio_type = {media::AudioCodec::kPCM_MULAW};
    return audio_type;
  } else if (config.codec() == "alaw") {
    audio_type = {media::AudioCodec::kPCM_ALAW};
    return audio_type;
  }

  // https://www.w3.org/TR/webcodecs-flac-codec-registration
  // https://www.w3.org/TR/webcodecs-vorbis-codec-registration
  bool description_required = false;
  if (config.codec() == "flac" || config.codec() == "vorbis")
    description_required = true;

  if (description_required && !config.hasDescription()) {
    *js_error_message = "Description is required.";
    return absl::nullopt;
  }

  media::AudioCodec codec = media::AudioCodec::kUnknown;
  bool is_codec_ambiguous = true;
  const bool parse_succeeded = ParseAudioCodecString(
      "", config.codec().Utf8(), &is_codec_ambiguous, &codec);

  if (!parse_succeeded) {
    *js_error_message = "Failed to parse codec string.";
    return absl::nullopt;
  }

  if (is_codec_ambiguous) {
    *js_error_message = "Codec string is ambiguous.";
    return absl::nullopt;
  }

  audio_type = {codec};
  return audio_type;
}

// static
absl::optional<media::AudioDecoderConfig>
AudioDecoder::MakeMediaAudioDecoderConfig(const ConfigType& config,
                                          String* js_error_message) {
  absl::optional<media::AudioType> audio_type =
      IsValidAudioDecoderConfig(config, js_error_message);
  if (!audio_type)
    return absl::nullopt;

  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    // TODO(crbug.com/1179970): This should throw if description is detached.
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.empty()) {
      const uint8_t* start = desc_wrapper.data();
      const size_t size = desc_wrapper.size();
      extra_data.assign(start, start + size);
    }
  }

  media::ChannelLayout channel_layout =
      config.numberOfChannels() > 8
          // GuesschannelLayout() doesn't know how to guess above 8 channels.
          ? media::CHANNEL_LAYOUT_DISCRETE
          : media::GuessChannelLayout(config.numberOfChannels());

  // TODO(chcunningham): Add sample format to IDL.
  media::AudioDecoderConfig media_config;
  media_config.Initialize(
      audio_type->codec, media::kSampleFormatPlanarF32, channel_layout,
      config.sampleRate(), extra_data, media::EncryptionScheme::kUnencrypted,
      base::TimeDelta() /* seek preroll */, 0 /* codec delay */);

  return media_config;
}

AudioDecoder::AudioDecoder(ScriptState* script_state,
                           const AudioDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<AudioDecoderTraits>(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

bool AudioDecoder::IsValidConfig(const ConfigType& config,
                                 String* js_error_message) {
  return IsValidAudioDecoderConfig(config, js_error_message /* out */)
      .has_value();
}

absl::optional<media::AudioDecoderConfig> AudioDecoder::MakeMediaConfig(
    const ConfigType& config,
    String* js_error_message) {
  DCHECK(js_error_message);
  return MakeMediaAudioDecoderConfig(config, js_error_message /* out */);
}

media::DecoderStatus::Or<scoped_refptr<media::DecoderBuffer>>
AudioDecoder::MakeInput(const InputType& chunk, bool verify_key_frame) {
  if (verify_key_frame && !chunk.buffer()->is_key_frame())
    return media::DecoderStatus::Codes::kKeyFrameRequired;
  return chunk.buffer();
}

media::DecoderStatus::Or<AudioDecoder::OutputType*> AudioDecoder::MakeOutput(
    scoped_refptr<MediaOutputType> output,
    ExecutionContext* context) {
  if (!blink::audio_utilities::IsValidAudioBufferSampleRate(
          output->sample_rate())) {
    return media::DecoderStatus(
        media::DecoderStatus::Codes::kInvalidArgument,
        String::Format("Invalid decoded audio output sample rate. Got %u, "
                       "which is outside [%f, %f]",
                       output->sample_rate(),
                       blink::audio_utilities::MinAudioBufferSampleRate(),
                       blink::audio_utilities::MaxAudioBufferSampleRate())
            .Ascii());
  }

  if (static_cast<uint32_t>(output->channel_count()) >
      BaseAudioContext::MaxNumberOfChannels()) {
    return media::DecoderStatus(
        media::DecoderStatus::Codes::kInvalidArgument,
        String::Format("Invalid decoded audio output channel "
                       "count. Got %u, which exceeds %u",
                       output->channel_count(),
                       BaseAudioContext::MaxNumberOfChannels())
            .Ascii());
  }

  return MakeGarbageCollected<AudioDecoderTraits::OutputType>(
      std::move(output));
}

const AtomicString& AudioDecoder::InterfaceName() const {
  return event_target_names::kAudioDecoder;
}

}  // namespace blink
