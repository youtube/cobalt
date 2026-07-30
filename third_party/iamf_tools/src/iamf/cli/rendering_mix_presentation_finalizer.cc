/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/rendering_mix_presentation_finalizer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using LayoutRenderingMetadata =
    RenderingMixPresentationFinalizer::LayoutRenderingMetadata;
using SubmixRenderingMetadata =
    RenderingMixPresentationFinalizer::SubmixRenderingMetadata;

bool CanRenderAnyLayout(
    const std::vector<SubmixRenderingMetadata>& rendering_metadata) {
  for (auto& submix_rendering_metadata : rendering_metadata) {
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (layout_rendering_metadata.can_render) {
        return true;
      }
    }
  }
  return false;
}

absl::Status CollectAudioElementsInSubMix(
    const DescriptorObus::AudioElementsById& audio_elements,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix) {
  audio_elements_in_sub_mix.reserve(sub_mix_audio_elements.size());
  for (const auto& audio_element : sub_mix_audio_elements) {
    auto iter = audio_elements.find(audio_element.audio_element_id);
    if (iter == audio_elements.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Audio Element with ID= ", audio_element.audio_element_id,
          " not found"));
    }
    audio_elements_in_sub_mix.push_back(&iter->second);
  }

  return absl::OkStatus();
}

absl::Status GetCommonCodecConfigPropertiesFromAudioElementIds(
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    uint32_t& common_num_samples_per_frame, bool& requires_resampling) {
  absl::flat_hash_set<uint32_t> sample_rates;
  absl::flat_hash_set<uint32_t> num_samples_per_frame;
  absl::flat_hash_set<uint8_t> bit_depths;

  // Get all the bit-depths and sample_rates from each Audio Element.
  for (const auto* audio_element : audio_elements_in_sub_mix) {
    num_samples_per_frame.insert(
        audio_element->codec_config->GetNumSamplesPerFrame());
    sample_rates.insert(audio_element->codec_config->GetOutputSampleRate());
    bit_depths.insert(
        audio_element->codec_config->GetBitDepthToMeasureLoudness());
  }

  RETURN_IF_NOT_OK(GetCommonSampleRateAndBitDepth(
      sample_rates, bit_depths, common_sample_rate, common_bit_depth,
      requires_resampling));
  if (num_samples_per_frame.size() != 1) {
    return absl::InvalidArgumentError(
        "Audio elements in a submix must have the same number of samples per "
        "frame.");
  }
  common_num_samples_per_frame = *num_samples_per_frame.begin();

  return absl::OkStatus();
}

absl::Status InitializeRenderers(
    const RendererFactoryBase& renderer_factory,
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const Layout& loudness_layout, const uint32_t common_sample_rate,
    std::vector<std::unique_ptr<AudioElementRendererBase>>& renderers) {
  renderers.resize(audio_elements_in_sub_mix.size());

  for (size_t i = 0; i < audio_elements_in_sub_mix.size(); i++) {
    const auto& sub_mix_audio_element = *audio_elements_in_sub_mix[i];
    renderers[i] = renderer_factory.CreateRendererForLayout(
        sub_mix_audio_element.obu.audio_substream_ids_,
        sub_mix_audio_element.substream_id_to_labels,
        sub_mix_audio_element.obu.GetAudioElementType(),
        sub_mix_audio_element.obu.config_,
        sub_mix_audio_elements[i].rendering_config, loudness_layout,
        static_cast<size_t>(
            sub_mix_audio_element.codec_config->GetNumSamplesPerFrame()),
        static_cast<size_t>(
            sub_mix_audio_element.codec_config->GetOutputSampleRate()));

    if (renderers[i] == nullptr) {
      return absl::UnimplementedError("Unable to create renderer.");
    }

    const uint32_t output_sample_rate =
        sub_mix_audio_element.codec_config->GetOutputSampleRate();
    if (common_sample_rate != output_sample_rate) {
      // Theoretically, we would have to resample this audio element to the
      // common sample rate. However, as of IAMF v1.1.0, the spec forbids
      // multiple Codec Config OBUs. This case is not possible to occur with a
      // single Codec Config OBU.
      return absl::UnimplementedError(
          absl::StrCat("OBUs with different sample rates not supported yet: (",
                       common_sample_rate, " != ", output_sample_rate, ")."));
    }
  }

  return absl::OkStatus();
}

absl::Status FlushUntilNonEmptyOrTimeout(
    AudioElementRendererBase& audio_element_renderer,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  static const int kMaxNumTries = 500;
  for (int i = 0; i < kMaxNumTries; i++) {
    audio_element_renderer.Flush(rendered_samples);
    if (std::none_of(rendered_samples.begin(), rendered_samples.end(),
                     [](const auto& channel) { return channel.empty(); })) {
      // Usually samples will be ready right away. So avoid sleeping.
      return absl::OkStatus();
    }
    absl::SleepFor(absl::Milliseconds(10));
  }
  return absl::DeadlineExceededError("Timed out waiting for samples.");
}

absl::Status RenderLabeledFrameToLayout(
    const LabeledFrame& labeled_frame, const CodecConfigObu& codec_config,
    AudioElementRendererBase& renderer,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  const auto num_time_ticks = renderer.RenderLabeledFrame(labeled_frame);

  if (!num_time_ticks.ok()) {
    return num_time_ticks.status();
  } else if (*num_time_ticks >
             static_cast<size_t>(codec_config.GetNumSamplesPerFrame())) {
    return absl::InvalidArgumentError("Too many samples in this frame");
  } else if (*num_time_ticks == 0) {
    // This was an empty frame. Flush to get the number of channels right even
    // when there is no actual sample.
    renderer.Flush(rendered_samples);
    return absl::OkStatus();
  }

  return FlushUntilNonEmptyOrTimeout(renderer, rendered_samples);
}

// Fills in the output `mix_gains` with the gain in Q7.8 format to apply at each
// tick.
absl::Status GetParameterBlockLinearMixGainsPerTick(
    uint32_t common_sample_rate,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain,
    std::vector<float>& linear_mix_gain_per_tick) {
  if (mix_gain.GetParameterRate() != common_sample_rate) {
    // TODO(b/283281856): Support resampling parameter blocks.
    return absl::UnimplementedError(
        "Parameter blocks that require resampling are not supported yet.");
  }

  // Initialize to the default gain value.
  std::fill(
      linear_mix_gain_per_tick.begin(), linear_mix_gain_per_tick.end(),
      std::pow(10.0f, mix_gain.default_mix_gain_.GetFloatingPoint() / 20.0f));
  auto parameter_block_iter =
      id_to_parameter_block.find(mix_gain.GetParameterId());
  if (parameter_block_iter == id_to_parameter_block.end()) {
    // Default mix gain will be used for this frame. Logic elsewhere validates
    // the rest of the audio frames have consistent coverage.
    return absl::OkStatus();
  }
  const auto& parameter_block = *parameter_block_iter->second;
  InternalTimestamp cur_tick = parameter_block.start_timestamp;
  // Process as many ticks as possible until all are found or the parameter
  // block ends.
  while (cur_tick < parameter_block.end_timestamp &&
         static_cast<size_t>(cur_tick - parameter_block.start_timestamp) <
             linear_mix_gain_per_tick.size()) {
    RETURN_IF_NOT_OK(parameter_block.obu->GetLinearMixGain(
        cur_tick - parameter_block.start_timestamp,
        linear_mix_gain_per_tick[cur_tick - parameter_block.start_timestamp]));
    cur_tick++;
  }
  return absl::OkStatus();
}

absl::Status GetAndApplyMixGain(
    uint32_t common_sample_rate,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain, int32_t num_channels,
    std::vector<float>& linear_mix_gain_per_tick,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual("rendered_samples",
                                              rendered_samples, num_channels));

  const auto num_ticks =
      rendered_samples.empty() ? 0 : rendered_samples[0].size();

  // Get the mix gain on a per tick basis from the parameter block.
  linear_mix_gain_per_tick.resize(num_ticks);
  RETURN_IF_NOT_OK(GetParameterBlockLinearMixGainsPerTick(
      common_sample_rate, id_to_parameter_block, mix_gain,
      linear_mix_gain_per_tick));

  if (!linear_mix_gain_per_tick.empty()) {
    ABSL_LOG_FIRST_N(INFO, 6) << " First tick in this frame has gain: "
                              << linear_mix_gain_per_tick.front();
  }

  for (auto& rendered_samples_for_channel : rendered_samples) {
    for (size_t tick = 0; tick < num_ticks; tick++) {
      // Apply the mix gain per tick to all channels.
      rendered_samples_for_channel[tick] *= linear_mix_gain_per_tick[tick];
    }
  }

  return absl::OkStatus();
}

absl::Status MixAudioElements(
    std::vector<std::vector<std::vector<InternalSampleType>>>&
        rendered_audio_elements,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  const auto num_audio_elements = rendered_audio_elements.size();
  const auto num_channels = rendered_audio_elements.empty()
                                ? 0
                                : rendered_audio_elements.front().size();
  const auto num_ticks =
      num_channels == 0 ? 0 : rendered_audio_elements.front().front().size();
  ABSL_CHECK_EQ(rendered_samples.size(), num_channels);
  for (auto& rendered_samples_for_channel : rendered_samples) {
    // To avoid re-allocations, `rendered_samples_for_channel` should already
    // be allocated with the maximum number of samples that it will hold, which
    // is the number of samples per frame. Rendering a partial (therefore
    // smaller) frame is allowed.
    ABSL_CHECK_GE(rendered_samples_for_channel.capacity(), num_ticks);
    rendered_samples_for_channel.assign(num_ticks, 0.0);
  }

  // Expect all frames have the same number of channels and all channels
  // have the same number of ticks.
  for (const auto& rendered_audio_element : rendered_audio_elements) {
    RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
        "rendered_audio_element", rendered_audio_element, num_channels));
    for (const auto& samples_for_channel : rendered_audio_element) {
      RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
          "samples_for_channel", samples_for_channel, num_ticks));
    }
  }

  for (size_t a = 0; a < num_audio_elements; a++) {
    const auto& rendered_samples_for_audio_element = rendered_audio_elements[a];
    for (size_t c = 0; c < num_channels; c++) {
      auto& rendered_samples_for_channel = rendered_samples[c];
      const auto& rendered_samples_for_audio_element_for_channel =
          rendered_samples_for_audio_element[c];
      for (size_t t = 0; t < num_ticks; t++) {
        // Sum all audio elements for this (channel, tick).
        rendered_samples_for_channel[t] +=
            rendered_samples_for_audio_element_for_channel[t];
      }
    }
  }

  return absl::OkStatus();
}

// Fills in `valid_rendered_samples` which is a view backed by
// `rendered_samples` of the ticks actually rendered.
absl::Status RenderAllFramesForLayout(
    int32_t num_channels,
    const std::vector<SubMixAudioElement> sub_mix_audio_elements,
    const MixGainParamDefinition& output_mix_gain,
    const IdLabeledFrameMap& id_to_labeled_frame,
    const std::vector<const CodecConfigObu*>& codec_configs_in_sub_mix,
    const std::vector<std::unique_ptr<AudioElementRendererBase>>& renderers,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const uint32_t common_sample_rate,
    std::vector<std::vector<InternalSampleType>>& rendered_samples,
    std::vector<absl::Span<const InternalSampleType>>& valid_rendered_samples) {
  // Each audio element rendered individually with `element_mix_gain` applied.
  // TODO(b/382197581): Avoid creating `rendered_audio_elements` and
  //                    `linear_mix_gain_per_tick` for each frame.
  std::vector<std::vector<std::vector<InternalSampleType>>>
      rendered_audio_elements(sub_mix_audio_elements.size());
  std::vector<float> linear_mix_gain_per_tick;
  for (size_t i = 0; i < sub_mix_audio_elements.size(); i++) {
    const SubMixAudioElement& sub_mix_audio_element = sub_mix_audio_elements[i];
    const auto audio_element_id = sub_mix_audio_element.audio_element_id;

    if (id_to_labeled_frame.find(audio_element_id) !=
        id_to_labeled_frame.end()) {
      const auto& labeled_frame = id_to_labeled_frame.at(audio_element_id);
      // Render the frame to the specified `loudness_layout` and apply element
      // mix gain.
      RETURN_IF_NOT_OK(RenderLabeledFrameToLayout(
          labeled_frame, *codec_configs_in_sub_mix[i], *renderers[i],
          rendered_audio_elements[i]));
    }

    RETURN_IF_NOT_OK(GetAndApplyMixGain(
        common_sample_rate, id_to_parameter_block,
        sub_mix_audio_element.element_mix_gain, num_channels,
        linear_mix_gain_per_tick, rendered_audio_elements[i]));
  }

  // Mix the audio elements.
  RETURN_IF_NOT_OK(MixAudioElements(rendered_audio_elements, rendered_samples));

  ABSL_LOG_FIRST_N(INFO, 1) << "    Applying output_mix_gain.default_mix_gain= "
                            << output_mix_gain.default_mix_gain_;

  RETURN_IF_NOT_OK(GetAndApplyMixGain(
      common_sample_rate, id_to_parameter_block, output_mix_gain, num_channels,
      linear_mix_gain_per_tick, rendered_samples));

  valid_rendered_samples.resize(rendered_samples.size());
  for (size_t c = 0; c < rendered_samples.size(); ++c) {
    valid_rendered_samples[c] = absl::MakeConstSpan(rendered_samples[c]);
  }

  return absl::OkStatus();
}

absl::Status ValidateUserLoudness(const LoudnessInfo& user_loudness,
                                  const uint32_t mix_presentation_id,
                                  const int sub_mix_index,
                                  const int layout_index,
                                  const LoudnessInfo& output_loudness,
                                  bool& loudness_matches_user_data) {
  const std::string mix_presentation_sub_mix_layout_index =
      absl::StrCat("Mix Presentation(ID ", mix_presentation_id, ")->sub_mixes[",
                   sub_mix_index, "]->layouts[", layout_index, "]: ");
  if (output_loudness.integrated_loudness !=
      user_loudness.integrated_loudness) {
    ABSL_LOG(ERROR) << mix_presentation_sub_mix_layout_index
                    << "Computed integrated loudness different from "
                    << "user specification: "
                    << output_loudness.integrated_loudness << " vs "
                    << user_loudness.integrated_loudness;
    loudness_matches_user_data = false;
  }

  if (output_loudness.digital_peak != user_loudness.digital_peak) {
    ABSL_LOG(ERROR) << mix_presentation_sub_mix_layout_index
                    << "Computed digital peak different from "
                    << "user specification: " << output_loudness.digital_peak
                    << " vs " << user_loudness.digital_peak;
    loudness_matches_user_data = false;
  }

  if (output_loudness.info_type & LoudnessInfo::kTruePeak) {
    if (output_loudness.true_peak != user_loudness.true_peak) {
      ABSL_LOG(ERROR) << mix_presentation_sub_mix_layout_index
                      << "Computed true peak different from "
                      << "user specification: " << output_loudness.true_peak
                      << " vs " << user_loudness.true_peak;
      loudness_matches_user_data = false;
    }
  }

  // Anchored loudness and layout extension are copied from the user input
  // and do not need to be validated.

  return absl::OkStatus();
}

// Calculates the loudness of the rendered samples. These rendered samples are
// for a specific timestamp for a given submix and layout. If
// `validate_loudness` is true, then the user provided loudness values are
// validated against the computed values.
absl::Status UpdateLoudnessInfoForLayout(
    bool validate_loudness, const LoudnessInfo& input_loudness,
    const uint32_t mix_presentation_id, const int sub_mix_index,
    const int layout_index, bool& loudness_matches_user_data,
    std::unique_ptr<LoudnessCalculatorBase> loudness_calculator,
    LoudnessInfo& output_calculated_loudness) {
  // Copy the final loudness values back to the output OBU.
  auto calculated_loudness_info = loudness_calculator->QueryLoudness();
  if (!calculated_loudness_info.ok()) {
    return calculated_loudness_info.status();
  }

  if (validate_loudness) {
    // Validate any user provided loudness values match computed values.
    RETURN_IF_NOT_OK(ValidateUserLoudness(
        input_loudness, mix_presentation_id, sub_mix_index, layout_index,
        *calculated_loudness_info, loudness_matches_user_data));
  }
  output_calculated_loudness = *calculated_loudness_info;
  return absl::OkStatus();
}

// Generates rendering metadata for all layouts within a submix. This includes
// optionally creating a sample processor and/or a loudness calculator for each
// layout.
absl::Status GenerateRenderingMetadataForLayouts(
    const RendererFactoryBase& renderer_factory,
    const LoudnessCalculatorFactoryBase* loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    const DecodedUleb128 mix_presentation_id,
    const MixPresentationSubMix& sub_mix, int sub_mix_index,
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    uint32_t common_sample_rate, uint8_t rendering_bit_depth,
    uint32_t common_num_samples_per_frame,
    std::vector<LayoutRenderingMetadata>& output_layout_rendering_metadata) {
  output_layout_rendering_metadata.resize(sub_mix.layouts.size());
  for (size_t layout_index = 0; layout_index < sub_mix.layouts.size();
       layout_index++) {
    LayoutRenderingMetadata& layout_rendering_metadata =
        output_layout_rendering_metadata[layout_index];
    const auto& layout = sub_mix.layouts[layout_index];

    int32_t num_channels = 0;
    auto can_render_status = MixPresentationObu::GetNumChannelsFromLayout(
        layout.loudness_layout, num_channels);
    layout_rendering_metadata.num_channels = num_channels;

    can_render_status.Update(InitializeRenderers(
        renderer_factory, audio_elements_in_sub_mix, sub_mix.audio_elements,
        layout.loudness_layout, common_sample_rate,
        layout_rendering_metadata.renderers));

    if (!can_render_status.ok()) {
      layout_rendering_metadata.can_render = false;
      continue;
    } else {
      layout_rendering_metadata.can_render = true;
    }
    if (loudness_calculator_factory != nullptr) {
      // Optionally create a loudness calculator.
      layout_rendering_metadata.loudness_calculator =
          loudness_calculator_factory->CreateLoudnessCalculator(
              layout, common_num_samples_per_frame, common_sample_rate);
    }
    // Optionally create a post-processor.
    layout_rendering_metadata.sample_processor = sample_processor_factory(
        mix_presentation_id, sub_mix_index, layout_index,
        layout.loudness_layout, num_channels, common_sample_rate,
        rendering_bit_depth, common_num_samples_per_frame);

    // Pre-allocate a buffer to store a frame's worth of rendered samples.
    layout_rendering_metadata.rendered_samples.assign(
        num_channels,
        std::vector<InternalSampleType>(common_num_samples_per_frame, 0.0));
  }

  return absl::OkStatus();
}

// We generate one rendering metadata object for each submix. Once this
// metadata is generated, we will loop through it to render all submixes
// for a given timestamp. Within a submix, there can be many different audio
// elements and layouts that need to be rendered as well. Not all of these
// need to be rendered; only the ones that either have a wav writer or a
// loudness calculator.
absl::Status GenerateRenderingMetadataForSubmixes(
    const RendererFactoryBase& renderer_factory,
    const LoudnessCalculatorFactoryBase* absl_nullable
        loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    const DescriptorObus::AudioElementsById& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    std::vector<SubmixRenderingMetadata>& output_rendering_metadata) {
  const auto mix_presentation_id = mix_presentation_obu.GetMixPresentationId();
  output_rendering_metadata.resize(mix_presentation_obu.sub_mixes_.size());
  for (size_t sub_mix_index = 0;
       sub_mix_index < mix_presentation_obu.sub_mixes_.size();
       ++sub_mix_index) {
    SubmixRenderingMetadata& submix_rendering_metadata =
        output_rendering_metadata[sub_mix_index];
    const auto& sub_mix = mix_presentation_obu.sub_mixes_[sub_mix_index];

    // Pointers to audio elements in this sub mix; useful later.
    std::vector<const AudioElementWithData*> audio_elements_in_sub_mix;
    RETURN_IF_NOT_OK(CollectAudioElementsInSubMix(
        audio_elements, sub_mix.audio_elements, audio_elements_in_sub_mix));

    submix_rendering_metadata.audio_elements_in_sub_mix =
        sub_mix.audio_elements;
    submix_rendering_metadata.mix_gain =
        std::make_unique<MixGainParamDefinition>(sub_mix.output_mix_gain);
    submix_rendering_metadata.codec_configs_in_sub_mix.reserve(
        audio_elements_in_sub_mix.size());
    for (const auto* audio_element : audio_elements_in_sub_mix) {
      submix_rendering_metadata.codec_configs_in_sub_mix.emplace_back(
          audio_element->codec_config);
    }

    // Data common to all audio elements and layouts.
    bool requires_resampling;
    uint32_t common_num_samples_per_frame;
    uint8_t rendering_bit_depth;
    RETURN_IF_NOT_OK(GetCommonCodecConfigPropertiesFromAudioElementIds(
        audio_elements_in_sub_mix, submix_rendering_metadata.common_sample_rate,
        rendering_bit_depth, common_num_samples_per_frame,
        requires_resampling));
    if (requires_resampling) {
      // Detected multiple Codec Config OBUs with different sample rates. The
      // IAMF spec forbids this in all known profiles.
      //
      // The spec implies this could be valid in the future, return an error
      // instead of trying to implement it.
      return absl::UnimplementedError(
          "This implementation does not support mixing Codec Config OBUs with "
          "different sample rates.");
    }
    RETURN_IF_NOT_OK(GenerateRenderingMetadataForLayouts(
        renderer_factory, loudness_calculator_factory, sample_processor_factory,
        mix_presentation_id, sub_mix, sub_mix_index, audio_elements_in_sub_mix,
        submix_rendering_metadata.common_sample_rate, rendering_bit_depth,
        common_num_samples_per_frame,
        submix_rendering_metadata.layout_rendering_metadata));
  }
  return absl::OkStatus();
}

absl::Status FlushPostProcessors(
    std::vector<SubmixRenderingMetadata>& rendering_metadata) {
  for (auto& submix_rendering_metadata : rendering_metadata) {
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (layout_rendering_metadata.sample_processor != nullptr) {
        RETURN_IF_NOT_OK(layout_rendering_metadata.sample_processor->Flush());
      }
    }
  }

  return absl::OkStatus();
}

absl::Status FillLoudnessForMixPresentation(
    bool validate_loudness,
    std::vector<SubmixRenderingMetadata>& rendering_metadata,
    MixPresentationObu& mix_presentation_obu) {
  bool loudness_matches_user_data = true;
  int submix_index = 0;
  for (auto& submix_rendering_metadata : rendering_metadata) {
    int layout_index = 0;
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (layout_rendering_metadata.loudness_calculator != nullptr) {
        RETURN_IF_NOT_OK(UpdateLoudnessInfoForLayout(
            validate_loudness,
            mix_presentation_obu.sub_mixes_[submix_index]
                .layouts[layout_index]
                .loudness,
            mix_presentation_obu.GetMixPresentationId(), submix_index,
            layout_index, loudness_matches_user_data,
            std::move(layout_rendering_metadata.loudness_calculator),
            mix_presentation_obu.sub_mixes_[submix_index]
                .layouts[layout_index]
                .loudness));
      }
      layout_index++;
    }
    submix_index++;
  }
  if (!loudness_matches_user_data) {
    return absl::InvalidArgumentError("Loudness does not match user data.");
  }
  return absl::OkStatus();
}

// Renders all submixes, layouts, and audio elements for a temporal unit. It
// then optionally writes the rendered samples to a wav file and/or calculates
// the loudness of the rendered samples.
absl::Status RenderWriteAndCalculateLoudnessForTemporalUnit(
    const IdLabeledFrameMap& id_to_labeled_frame,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    std::vector<SubmixRenderingMetadata>& rendering_metadata) {
  for (auto& submix_rendering_metadata : rendering_metadata) {
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (!layout_rendering_metadata.can_render) {
        continue;
      }
      if (submix_rendering_metadata.mix_gain == nullptr) {
        return absl::InvalidArgumentError("Submix mix gain is null");
      }

      RETURN_IF_NOT_OK(RenderAllFramesForLayout(
          layout_rendering_metadata.num_channels,
          submix_rendering_metadata.audio_elements_in_sub_mix,
          *submix_rendering_metadata.mix_gain, id_to_labeled_frame,
          submix_rendering_metadata.codec_configs_in_sub_mix,
          layout_rendering_metadata.renderers, id_to_parameter_block,
          submix_rendering_metadata.common_sample_rate,
          layout_rendering_metadata.rendered_samples,
          layout_rendering_metadata.valid_rendered_samples));
      auto span_of_valid_rendered_samples =
          absl::MakeSpan(layout_rendering_metadata.valid_rendered_samples);

      // Calculate loudness based on the original rendered samples; we do not
      // know what post-processing the end user will have.
      if (layout_rendering_metadata.loudness_calculator != nullptr) {
        RETURN_IF_NOT_OK(
            layout_rendering_metadata.loudness_calculator
                ->AccumulateLoudnessForSamples(span_of_valid_rendered_samples));
      }

      // Perform any post-processing.
      if (layout_rendering_metadata.sample_processor != nullptr) {
        RETURN_IF_NOT_OK(layout_rendering_metadata.sample_processor->PushFrame(
            span_of_valid_rendered_samples));
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<const LayoutRenderingMetadata*>
GetRenderedSamplesAndPostProcessor(
    const absl::flat_hash_map<DecodedUleb128,
                              std::vector<SubmixRenderingMetadata>>&
        mix_presentation_id_to_sub_mix_rendering_metadata,
    DecodedUleb128 mix_presentation_id, size_t sub_mix_index,
    size_t layout_index) {
  // Lookup the requested layout in the requested mix presentation.
  const auto sub_mix_rendering_metadata_it =
      mix_presentation_id_to_sub_mix_rendering_metadata.find(
          mix_presentation_id);
  const auto mix_presentation_id_error_message =
      absl::StrCat(" Mix Presentation ID ", mix_presentation_id);
  if (sub_mix_rendering_metadata_it ==
      mix_presentation_id_to_sub_mix_rendering_metadata.end()) {
    return absl::NotFoundError(
        absl::StrCat(mix_presentation_id_error_message,
                     " not found in rendering metadata."));
  }

  // Validate the sub mix and layout are in bounds, then retrieve it.
  const auto& [unused_mix_presentation_id, sub_mix_rendering_metadatas] =
      *sub_mix_rendering_metadata_it;
  RETURN_IF_NOT_OK(Validate(
      sub_mix_index, std::less<size_t>(), sub_mix_rendering_metadatas.size(),
      absl::StrCat(mix_presentation_id_error_message, "  sub_mix_index <")));
  RETURN_IF_NOT_OK(Validate(
      layout_index, std::less<size_t>(),
      sub_mix_rendering_metadatas[sub_mix_index]
          .layout_rendering_metadata.size(),
      absl::StrCat(mix_presentation_id_error_message, "  layout_index <")));
  return &sub_mix_rendering_metadatas[sub_mix_index]
              .layout_rendering_metadata[layout_index];
}

}  // namespace

absl::StatusOr<RenderingMixPresentationFinalizer>
RenderingMixPresentationFinalizer::Create(
    const RendererFactoryBase* absl_nullable renderer_factory,
    const LoudnessCalculatorFactoryBase* absl_nullable
        loudness_calculator_factory,
    const DescriptorObus::AudioElementsById& audio_elements,
    const SampleProcessorFactory& sample_processor_factory,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus) {
  const bool rendering_enabled = renderer_factory != nullptr;
  if (!rendering_enabled) {
    ABSL_LOG(INFO) << "Rendering is safely disabled.";
  }
  if (loudness_calculator_factory == nullptr) {
    ABSL_VLOG(1)
        << "Loudness calculator factory is null so loudness will not be "
           "calculated.";
  }
  absl::flat_hash_map<DecodedUleb128, std::vector<SubmixRenderingMetadata>>
      mix_presentation_id_to_rendering_metadata;
  DescriptorObus::MixPresentationObus mix_presentation_obus_to_render;
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    // Copy all mix presentation OBUs, so they can be echoed back, even when
    // rendering is disabled.
    mix_presentation_obus_to_render.emplace_back(mix_presentation_obu);

    // Fill in rendering metadata if rendering is enabled, and at least one
    // layout can be rendered.
    if (rendering_enabled) {
      std::vector<SubmixRenderingMetadata> temp_sub_mix_rendering_metadata;
      RETURN_IF_NOT_OK(GenerateRenderingMetadataForSubmixes(
          *renderer_factory, loudness_calculator_factory,
          sample_processor_factory, audio_elements, mix_presentation_obu,
          temp_sub_mix_rendering_metadata));
      if (CanRenderAnyLayout(temp_sub_mix_rendering_metadata)) {
        mix_presentation_id_to_rendering_metadata.emplace(
            mix_presentation_obu.GetMixPresentationId(),
            std::move(temp_sub_mix_rendering_metadata));
      }
    }
  }

  return RenderingMixPresentationFinalizer(
      std::move(mix_presentation_id_to_rendering_metadata),
      std::move(mix_presentation_obus_to_render));
}

absl::Status RenderingMixPresentationFinalizer::PushTemporalUnit(
    const IdLabeledFrameMap& id_to_labeled_frame,
    InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks) {
  switch (state_) {
    case kAcceptingTemporalUnits:
      // Ok to push.
      break;
    case kFinalizePushTemporalUnitCalled:
      return absl::FailedPreconditionError(
          "PushTemporalUnit() should not be called after "
          "FinalizePushingTemporalUnits() has been called.");
    case kFlushedFinalizedMixPresentationObus:
      return absl::FailedPreconditionError(
          "PushTemporalUnit() should not be called after "
          "GetFinalizedMixPresentationOBUs() has been called.");
  }

  // First organize parameter blocks by IDs.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      id_to_parameter_block;
  for (const auto& parameter_block : parameter_blocks) {
    RETURN_IF_NOT_OK(CompareTimestamps(
        start_timestamp, parameter_block.start_timestamp,
        "In PushTemporalUnit(), parameter blocks start time:"));
    RETURN_IF_NOT_OK(
        CompareTimestamps(end_timestamp, parameter_block.end_timestamp,
                          "In PushTemporalUnit(), parameter blocks end time:"));
    id_to_parameter_block[parameter_block.obu->parameter_id_] =
        &parameter_block;
  }
  for (auto& [mix_presentation_ids, sub_mix_rendering_metadata] :
       mix_presentation_id_to_sub_mix_rendering_metadata_) {
    RETURN_IF_NOT_OK(RenderWriteAndCalculateLoudnessForTemporalUnit(
        id_to_labeled_frame, id_to_parameter_block,
        sub_mix_rendering_metadata));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Span<const absl::Span<const InternalSampleType>>>
RenderingMixPresentationFinalizer::GetPostProcessedSamplesAsSpan(
    DecodedUleb128 mix_presentation_id, size_t sub_mix_index,
    size_t layout_index) const {
  const auto layout_rendering_metadata = GetRenderedSamplesAndPostProcessor(
      mix_presentation_id_to_sub_mix_rendering_metadata_, mix_presentation_id,
      sub_mix_index, layout_index);
  if (!layout_rendering_metadata.ok()) {
    return layout_rendering_metadata.status();
  }
  // `absl::StatusOr<const T*> cannot hold a nullptr.
  ABSL_CHECK_NE(*layout_rendering_metadata, nullptr);

  // Prioritize returning the post-processed samples if a post-processor is
  // available. Otherwise, return the rendered samples.
  return (*layout_rendering_metadata)->sample_processor != nullptr
             ? (*layout_rendering_metadata)
                   ->sample_processor->GetOutputSamplesAsSpan()
             : absl::MakeSpan(
                   (*layout_rendering_metadata)->valid_rendered_samples);
}

absl::Status RenderingMixPresentationFinalizer::FinalizePushingTemporalUnits() {
  switch (state_) {
    case kAcceptingTemporalUnits:
      state_ = kFinalizePushTemporalUnitCalled;
      break;
    case kFinalizePushTemporalUnitCalled:
    case kFlushedFinalizedMixPresentationObus:
      return absl::FailedPreconditionError(
          "FinalizePushingTemporalUnits() should not be called twice.");
  }

  for (auto& [mix_presentation_id, sub_mix_rendering_metadata] :
       mix_presentation_id_to_sub_mix_rendering_metadata_) {
    RETURN_IF_NOT_OK(FlushPostProcessors(sub_mix_rendering_metadata));
  }
  return absl::OkStatus();
}

absl::StatusOr<DescriptorObus::MixPresentationObus>
RenderingMixPresentationFinalizer::GetFinalizedMixPresentationObus(
    bool validate_loudness) {
  switch (state_) {
    case kAcceptingTemporalUnits:
      return absl::FailedPreconditionError(
          "FinalizePushingTemporalUnits() should be called before "
          "GetFinalizedMixPresentationOBUs().");
    case kFinalizePushTemporalUnitCalled:
      // Ok to finalize.
      break;
    case kFlushedFinalizedMixPresentationObus:
      return absl::FailedPreconditionError(
          "GetFinalizedMixPresentationOBUs() should not be called twice.");
  }

  // Finalize the OBUs in place.
  for (auto& mix_presentation_obu : mix_presentation_obus_) {
    const auto sub_mix_rendering_metadata_it =
        mix_presentation_id_to_sub_mix_rendering_metadata_.find(
            mix_presentation_obu.GetMixPresentationId());
    if (sub_mix_rendering_metadata_it ==
        mix_presentation_id_to_sub_mix_rendering_metadata_.end()) {
      ABSL_LOG(INFO) << "Rendering was disabled for Mix Presentation ID= "
                     << mix_presentation_obu.GetMixPresentationId()
                     << " echoing the input OBU.";
      continue;
    }

    RETURN_IF_NOT_OK(FillLoudnessForMixPresentation(
        validate_loudness, sub_mix_rendering_metadata_it->second,
        mix_presentation_obu));
    mix_presentation_obu.PrintObu();
  }

  // Flush the finalized OBUs and mark that this class should not use them
  // again.
  state_ = kFlushedFinalizedMixPresentationObus;
  return std::move(mix_presentation_obus_);
}

}  // namespace iamf_tools
