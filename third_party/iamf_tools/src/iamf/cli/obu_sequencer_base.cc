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
#include "iamf/cli/obu_sequencer_base.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/profile_filter.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using absl::MakeConstSpan;

// Write buffer. Let's start with 64 KB. The buffer will resize for larger
// OBUs if needed.
constexpr int64_t kBufferStartSize = 65536;

/*!\brief Helper class to abort an `ObuSequencerBase` on destruction.
 *
 * This class calls on an `ObuSequencerBase::Abort` on destruction. Or does
 * nothing if `CancelAbort` is called.
 *
 * Typically, this is useful to create an instance of this class on the stack,
 * in the scope of a function which has many locations where it may return an
 * un-recoverable error. When those exit points are reached, the sequencer will
 * automatically be aborted.
 *
 * Before any successful exit point, `CancelAbort` should be called, which will
 * prevent the sequencer from being aborting.
 */
class AbortOnDestruct {
 public:
  /*!\brief Constructor
   *
   * \param obu_sequencer The `ObuSequencerBase` to abort on destruction.
   */
  explicit AbortOnDestruct(ObuSequencerBase* obu_sequencer)
      : obu_sequencer(obu_sequencer) {}

  /*!\brief Destructor */
  ~AbortOnDestruct() {
    if (obu_sequencer != nullptr) {
      obu_sequencer->Abort();
    }
  }

  /*!\brief Cancels the abort on destruction. */
  void CancelAbort() { obu_sequencer = nullptr; }

 private:
  ObuSequencerBase* obu_sequencer;
};

template <typename KeyValueMap, typename KeyComparator>
std::vector<typename KeyValueMap::key_type> SortedKeys(
    const KeyValueMap& map, const KeyComparator& comparator) {
  std::vector<typename KeyValueMap::key_type> keys;
  keys.reserve(map.size());
  for (const auto& [key, value] : map) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end(), comparator);
  return keys;
}
// Some IA Sequences can be "trivial" and missing descriptor OBUs or audio
// frames. These would decode to an empty stream. Fallback to some reasonable,
// but arbitrary default values, when the true value is undefined.

// Fallback number of samples per frame when there are no audio frames.
constexpr uint32_t kFallbackSamplesPerFrame = 1024;
// Fallback sample rate when there are no Codec Config OBUs.
constexpr uint32_t kFallbackSampleRate = 48000;
// Fallback bit-depth when there are no Codec Config OBUs.
constexpr uint8_t kFallbackBitDepth = 16;
// Fallback number of channels when there are no audio elements.
constexpr uint32_t kFallbackNumChannels = 2;
// Fallback first PTS when there are no audio frames.
constexpr int64_t kFallbackFirstPts = 0;

// Gets the common sample rate and bit depth for the given codec config OBUs. Or
// falls back to default values if there are no codec configs.
absl::Status GetCommonSampleRateAndBitDepth(
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling) {
  if (codec_config_obus.empty()) {
    // The true value is undefined, but the muxer requires non-zero values.
    common_sample_rate = kFallbackSampleRate;
    common_bit_depth = kFallbackBitDepth;
    requires_resampling = false;
    return absl::OkStatus();
  }

  requires_resampling = false;
  absl::flat_hash_set<uint32_t> sample_rates;
  absl::flat_hash_set<uint8_t> bit_depths;
  for (const auto& [unused_id, obu] : codec_config_obus) {
    sample_rates.insert(obu.GetOutputSampleRate());
    bit_depths.insert(obu.GetBitDepthToMeasureLoudness());
  }

  return ::iamf_tools::GetCommonSampleRateAndBitDepth(
      sample_rates, bit_depths, common_sample_rate, common_bit_depth,
      requires_resampling);
}

absl::Status WriteObusWithHook(
    ArbitraryObu::InsertionHook insertion_hook,
    const std::vector<const ArbitraryObu*>& arbitrary_obus,
    WriteBitBuffer& wb) {
  for (const auto& arbitrary_obu : arbitrary_obus) {
    if (arbitrary_obu->insertion_hook_ == insertion_hook) {
      RETURN_IF_NOT_OK(arbitrary_obu->ValidateAndWriteObu(wb));
    }
  }
  return absl::OkStatus();
}

absl::Status FillDescriptorStatistics(
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    auto& descriptor_statistics) {
  descriptor_statistics.common_samples_per_frame = kFallbackSamplesPerFrame;
  descriptor_statistics.common_sample_rate = kFallbackSampleRate;
  descriptor_statistics.common_bit_depth = kFallbackBitDepth;
  descriptor_statistics.num_channels = kFallbackNumChannels;

  bool requires_resampling = false;
  RETURN_IF_NOT_OK(GetCommonSampleRateAndBitDepth(
      codec_config_obus, descriptor_statistics.common_sample_rate,
      descriptor_statistics.common_bit_depth, requires_resampling));
  if (requires_resampling) {
    return absl::UnimplementedError(
        "Codec Config OBUs with different bit-depths and/or sample "
        "rates are not in base-enhanced/base/simple profile; they are not "
        "allowed in ISOBMFF.");
  }

  // This assumes all Codec Configs have the same sample rate and frame size.
  // We may need to be more careful if IA Samples do not all (except the
  // final) have the same duration in the future.
  return GetCommonSamplesPerFrame(
      codec_config_obus, descriptor_statistics.common_samples_per_frame);
}

absl::Status WriteTemporalUnit(bool include_temporal_delimiters,
                               const TemporalUnitView& temporal_unit,
                               WriteBitBuffer& wb, int& num_samples) {
  num_samples += temporal_unit.num_untrimmed_samples_;

  if (include_temporal_delimiters) {
    // Temporal delimiter has no payload.
    const TemporalDelimiterObu obu((ObuHeader()));
    RETURN_IF_NOT_OK(obu.ValidateAndWriteObu(wb));
  }

  RETURN_IF_NOT_OK(
      WriteObusWithHook(ArbitraryObu::kInsertionHookBeforeParameterBlocksAtTick,
                        temporal_unit.arbitrary_obus_, wb));

  // Write the Parameter Block OBUs.
  for (const auto& parameter_blocks : temporal_unit.parameter_blocks_) {
    const auto& parameter_block = parameter_blocks;
    RETURN_IF_NOT_OK(parameter_block->obu->ValidateAndWriteObu(wb));
  }

  RETURN_IF_NOT_OK(
      WriteObusWithHook(ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                        temporal_unit.arbitrary_obus_, wb));

  // Write Audio Frame OBUs.
  for (const auto& audio_frame : temporal_unit.audio_frames_) {
    RETURN_IF_NOT_OK(audio_frame->obu.ValidateAndWriteObu(wb));
    ABSL_LOG_FIRST_N(INFO, 10)
        << "wb.bit_offset= " << wb.bit_offset() << " after Audio Frame";
  }

  RETURN_IF_NOT_OK(
      WriteObusWithHook(ArbitraryObu::kInsertionHookAfterAudioFramesAtTick,
                        temporal_unit.arbitrary_obus_, wb));

  if (!wb.IsByteAligned()) {
    return absl::InvalidArgumentError("Write buffer not byte-aligned");
  }

  return absl::OkStatus();
}

// Writes the descriptor OBUs. Section 5.1.1
// (https://aomediacodec.github.io/iamf/#standalone-descriptor-obus) orders the
// OBUs by type.
//
// For Codec Config OBUs and Audio Element OBUs, the order is arbitrary. For
// determinism this implementation orders them by ascending ID.
//
// For Mix Presentation OBUs, the order is the same as the original order.
// Because the original ordering may be used downstream when selecting the mix
// presentation
// (https://aomediacodec.github.io/iamf/#processing-mixpresentation-selection).
//
// For Arbitrary OBUs, they are inserted in an order implied by the insertion
// hook. Ties are broken by the original order, when multiple OBUs have the same
// hook.
absl::Status WriteDescriptorObus(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const std::list<MetadataObu>& metadata_obus,
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    const std::list<ArbitraryObu>& arbitrary_obus, WriteBitBuffer& wb) {
  // Write IA Sequence Header OBU.
  RETURN_IF_NOT_OK(ia_sequence_header_obu.ValidateAndWriteObu(wb));
  ABSL_LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
                 << " after IA Sequence Header";

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterIaSequenceHeader, arbitrary_obus, wb));

  // Write Metadata OBUs.
  for (const auto& metadata_obu : metadata_obus) {
    RETURN_IF_NOT_OK(metadata_obu.ValidateAndWriteObu(wb));
    ABSL_LOG(INFO) << "wb.bit_offset= " << wb.bit_offset() << " after Metadata";
  }

  // Write Codec Config OBUs in ascending order of Codec Config IDs.
  // TODO(b/332956880): Support customizing the ordering.
  const std::vector<DecodedUleb128> codec_config_ids =
      SortedKeys(codec_config_obus, std::less<DecodedUleb128>());
  for (const auto id : codec_config_ids) {
    RETURN_IF_NOT_OK(codec_config_obus.at(id).ValidateAndWriteObu(wb));
    ABSL_LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
                   << " after Codec Config";
  }

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterCodecConfigs, arbitrary_obus, wb));

  // Write Audio Element OBUs in ascending order of Audio Element IDs.
  // TODO(b/332956880): Support customizing the ordering.
  const std::vector<DecodedUleb128> audio_element_ids =
      SortedKeys(audio_elements, std::less<DecodedUleb128>());
  for (const auto id : audio_element_ids) {
    RETURN_IF_NOT_OK(audio_elements.at(id).obu.ValidateAndWriteObu(wb));
    ABSL_LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
                   << " after Audio Element";
  }

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterAudioElements, arbitrary_obus, wb));

  // TODO(b/269708630): Ensure at least one the profiles in the IA Sequence
  //                    Header supports all of the layers for scalable audio
  //                    elements.
  // Maintain the original order of Mix Presentation OBUs.
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    // Make sure the mix presentation is valid for at least one of the profiles
    // in the sequence header before writing it.
    absl::flat_hash_set<ProfileVersion> profile_version = {
        ia_sequence_header_obu.GetPrimaryProfile(),
        ia_sequence_header_obu.GetAdditionalProfile()};
    RETURN_IF_NOT_OK(ProfileFilter::FilterProfilesForMixPresentation(
        audio_elements, mix_presentation_obu, profile_version));

    RETURN_IF_NOT_OK(mix_presentation_obu.ValidateAndWriteObu(wb));
    ABSL_LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
                   << " after Mix Presentation";
  }
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterMixPresentations, arbitrary_obus, wb));

  return absl::OkStatus();
}

}  // namespace

ObuSequencerBase::ObuSequencerBase(
    const LebGenerator& leb_generator, bool include_temporal_delimiters,
    bool delay_descriptors_until_first_untrimmed_sample)
    : leb_generator_(leb_generator),
      delay_descriptors_until_first_untrimmed_sample_(
          delay_descriptors_until_first_untrimmed_sample),
      include_temporal_delimiters_(include_temporal_delimiters),
      wb_(kBufferStartSize, leb_generator) {}

ObuSequencerBase::~ObuSequencerBase() {
  switch (state_) {
    case kInitialized:
      return;
    case kPushDescriptorObusCalled:
    case kPushSerializedDescriptorsCalled:
      ABSL_LOG(ERROR)
          << "OBUs have been pushed, but `ObuSequencerBase` is being "
             "destroyed without calling `Close` or `Abort`.";
      return;
    case kClosed:
      return;
  }
  // The above switch is exhaustive.
  ABSL_LOG(FATAL) << "Unexpected state: " << static_cast<int>(state_);
};

absl::Status ObuSequencerBase::PushDescriptorObus(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const std::list<MetadataObu>& metadata_obus,
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  // Many failure points should call `Abort`. We want to avoid leaving
  // sequencers open if they may have invalid or corrupted IAMF data.
  AbortOnDestruct abort_on_destruct(this);
  switch (state_) {
    case kInitialized:
      break;
    case kPushDescriptorObusCalled:
    case kPushSerializedDescriptorsCalled:
      return absl::FailedPreconditionError(
          "`PushDescriptorObus` can only be called once.");
    case kClosed:
      return absl::FailedPreconditionError(
          "`PushDescriptorObus` cannot be called after `Close` or `Abort`.");
  }
  state_ = kPushDescriptorObusCalled;
  wb_.Reset();

  // Serialize descriptor OBUS and adjacent arbitrary OBUs.
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookBeforeDescriptors, arbitrary_obus, wb_));
  // Write out the descriptor OBUs.
  RETURN_IF_NOT_OK(WriteDescriptorObus(
      ia_sequence_header_obu, metadata_obus, codec_config_obus, audio_elements,
      mix_presentation_obus, arbitrary_obus, wb_));
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterDescriptors, arbitrary_obus, wb_));
  // Cache the descriptor OBUs, so we can validate "functional" equivalence if
  // the user calls `UpdateDescriptorObusAndClose`.
  DescriptorStatistics descriptor_statistics{.descriptor_obus =
                                                 wb_.bit_buffer()};
  RETURN_IF_NOT_OK(
      FillDescriptorStatistics(codec_config_obus, descriptor_statistics));
  descriptor_statistics_.emplace(std::move(descriptor_statistics));

  if (!delay_descriptors_until_first_untrimmed_sample_) {
    // Avoid unnecessary delay, for concrete classes that don't need
    // `first_pts`.
    RETURN_IF_NOT_OK(PushSerializedDescriptorObus(
        descriptor_statistics_->common_samples_per_frame,
        descriptor_statistics_->common_sample_rate,
        descriptor_statistics_->common_bit_depth,
        descriptor_statistics_->first_untrimmed_timestamp,
        descriptor_statistics_->num_channels,
        descriptor_statistics_->descriptor_obus));

    state_ = kPushSerializedDescriptorsCalled;
  }

  abort_on_destruct.CancelAbort();
  return absl::OkStatus();
}

absl::Status ObuSequencerBase::PushTemporalUnit(
    const TemporalUnitView& temporal_unit) {
  // Many failure points should call `Abort`. We want to avoid leaving
  // sequencers open if they may have invalid or corrupted IAMF data.
  AbortOnDestruct abort_on_destruct(this);
  switch (state_) {
    case kInitialized:
      return absl::FailedPreconditionError(
          "PushDescriptorObus must be called before PushTemporalUnit.");
      break;
    case kPushDescriptorObusCalled:
    case kPushSerializedDescriptorsCalled:
      break;
    case kClosed:
      return absl::FailedPreconditionError(
          "PushTemporalUnit can only be called before `Close` or `Abort`.");
  }
  wb_.Reset();

  // Cache the frame for later
  const InternalTimestamp start_timestamp = temporal_unit.start_timestamp_;
  int num_samples = 0;
  RETURN_IF_NOT_OK(WriteTemporalUnit(include_temporal_delimiters_,
                                     temporal_unit, wb_, num_samples));
  cumulative_num_samples_for_logging_ += num_samples;
  num_temporal_units_for_logging_++;

  // TODO(b/476091301): Check if this is a key frame based on parameter blocks.
  const bool is_key_frame =
      std::any_of(temporal_unit.arbitrary_obus_.begin(),
                  temporal_unit.arbitrary_obus_.end(),
                  [](const ArbitraryObu* arbitrary_obu) {
                    return arbitrary_obu->header_.GetIsKeyFrame();
                  });

  if (!descriptor_statistics_->first_untrimmed_timestamp.has_value()) {
    // Treat the initial temporal units as a special case, this helps gather
    // statistics about the first untrimmed sample.
    RETURN_IF_NOT_OK(HandleInitialTemporalUnits(
        temporal_unit, absl::MakeConstSpan(wb_.bit_buffer()), is_key_frame));

  } else if (temporal_unit.num_samples_to_trim_at_start_ > 0) {
    return absl::InvalidArgumentError(
        "A unit has samples to trim at start, but the first untrimmed sample "
        "was already found.");
  } else [[likely]] {
    // This is by far the most common case, after we have seen the first real
    // frame of audio, we can handle this simply.
    RETURN_IF_NOT_OK(
        PushSerializedTemporalUnit(start_timestamp, num_samples, is_key_frame,
                                   absl::MakeConstSpan(wb_.bit_buffer())));
  }

  abort_on_destruct.CancelAbort();
  return absl::OkStatus();
}

absl::Status ObuSequencerBase::UpdateDescriptorObusAndClose(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const std::list<MetadataObu>& metadata_obus,
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  // Many failure points should call `Abort`. We want to avoid leaving
  // sequencers open if they may have invalid or corrupted IAMF data.
  AbortOnDestruct abort_on_destruct(this);
  switch (state_) {
    case kInitialized:
      return absl::FailedPreconditionError(
          "`UpdateDescriptorObusAndClose` must be called after "
          "`PushDescriptorObus`.");
    case kPushDescriptorObusCalled:
    case kPushSerializedDescriptorsCalled:
      break;
    case kClosed:
      return absl::FailedPreconditionError(
          "`Abort` or `Close` previously called.");
  }
  wb_.Reset();

  // Serialize descriptor OBUS and adjacent arbitrary OBUs.
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookBeforeDescriptors, arbitrary_obus, wb_));
  // Write out the descriptor OBUs.
  RETURN_IF_NOT_OK(WriteDescriptorObus(
      ia_sequence_header_obu, metadata_obus, codec_config_obus, audio_elements,
      mix_presentation_obus, arbitrary_obus, wb_));
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterDescriptors, arbitrary_obus, wb_));
  const auto updated_descriptor_obus = MakeConstSpan(wb_.bit_buffer());
  if (updated_descriptor_obus != descriptor_statistics_->descriptor_obus) {
    // Descriptors changed. We're a bit loose with what types of metadata we
    // allow to change. Check at least the "functional" statistics are
    // equivalent.
    DescriptorStatistics descriptor_statistics{
        .descriptor_obus = std::vector<uint8_t>(updated_descriptor_obus.begin(),
                                                updated_descriptor_obus.end())};

    RETURN_IF_NOT_OK(
        FillDescriptorStatistics(codec_config_obus, descriptor_statistics));
    if (descriptor_statistics_->common_samples_per_frame !=
            descriptor_statistics.common_samples_per_frame ||
        descriptor_statistics_->common_sample_rate !=
            descriptor_statistics.common_sample_rate ||
        descriptor_statistics_->common_bit_depth !=
            descriptor_statistics.common_bit_depth ||
        descriptor_statistics_->num_channels !=
            descriptor_statistics.num_channels) {
      return absl::FailedPreconditionError(
          "Descriptor OBUs have changed properties between finalizing and "
          "closing.");
    }
    if (descriptor_statistics_->descriptor_obus.size() !=
        descriptor_statistics.descriptor_obus.size()) {
      return absl::UnimplementedError(
          "Descriptor OBUs have changed size between finalizing and closing.");
    }

    RETURN_IF_NOT_OK(PushFinalizedDescriptorObus(updated_descriptor_obus));
    state_ = kPushSerializedDescriptorsCalled;
  }
  // OK, regardless of whether the descriptors actually changed, obey the
  // request to close.

  RETURN_IF_NOT_OK(Close());

  abort_on_destruct.CancelAbort();
  return absl::OkStatus();
}

absl::Status ObuSequencerBase::Close() {
  switch (state_) {
    case kInitialized:
      break;
    case kPushDescriptorObusCalled: {
      // Ok, trivial IA sequences don't have a first untrimmed timestamp. So
      // we will simply push the descriptors with a fallback PTS of 0.
      descriptor_statistics_->first_untrimmed_timestamp = kFallbackFirstPts;

      RETURN_IF_NOT_OK(PushSerializedDescriptorObus(
          descriptor_statistics_->common_samples_per_frame,
          descriptor_statistics_->common_sample_rate,
          descriptor_statistics_->common_bit_depth,
          descriptor_statistics_->first_untrimmed_timestamp,
          descriptor_statistics_->num_channels,
          descriptor_statistics_->descriptor_obus));
      state_ = kPushSerializedDescriptorsCalled;
      break;
    }
    case kPushSerializedDescriptorsCalled:
      break;
    case kClosed:
      return absl::FailedPreconditionError(
          "`Abort` or `Close` previously called.");
  }
  CloseDerived();
  state_ = kClosed;
  return absl::OkStatus();
}

void ObuSequencerBase::Abort() {
  AbortDerived();
  state_ = kClosed;
}

absl::Status ObuSequencerBase::HandleInitialTemporalUnits(
    const TemporalUnitView& temporal_unit,
    absl::Span<const uint8_t> serialized_temporal_unit, bool is_key_frame) {
  const bool found_first_untrimmed_sample =
      temporal_unit.num_untrimmed_samples_ != 0;
  if (found_first_untrimmed_sample) {
    // Gather the PTS. For internal accuracy, we store this even if we don't
    // need to delay the descriptors.
    descriptor_statistics_->first_untrimmed_timestamp =
        temporal_unit.start_timestamp_ +
        temporal_unit.num_samples_to_trim_at_start_;
  }

  // Push immediately if we don't need to delay the descriptors.
  if (!delay_descriptors_until_first_untrimmed_sample_) {
    return PushSerializedTemporalUnit(temporal_unit.start_timestamp_,
                                      temporal_unit.num_untrimmed_samples_,
                                      is_key_frame, serialized_temporal_unit);
  }

  if (!found_first_untrimmed_sample) {
    // This frame is fully trimmed. Cache it for later.
    delayed_temporal_units_.push_back(SerializedTemporalUnit{
        .start_timestamp = temporal_unit.start_timestamp_,
        .num_untrimmed_samples = temporal_unit.num_untrimmed_samples_,
        .is_key_frame = is_key_frame,
        .data = std::vector<uint8_t>(serialized_temporal_unit.begin(),
                                     serialized_temporal_unit.end())});
    return absl::OkStatus();
  }

  // Found the first untrimmed sample. Push out all delayed OBUs.
  RETURN_IF_NOT_OK(PushSerializedDescriptorObus(
      descriptor_statistics_->common_samples_per_frame,
      descriptor_statistics_->common_sample_rate,
      descriptor_statistics_->common_bit_depth,
      descriptor_statistics_->first_untrimmed_timestamp,
      descriptor_statistics_->num_channels,
      descriptor_statistics_->descriptor_obus));
  state_ = kPushSerializedDescriptorsCalled;

  // Flush any delayed temporal units.
  for (const auto& delayed_temporal_unit : delayed_temporal_units_) {
    RETURN_IF_NOT_OK(PushSerializedTemporalUnit(
        delayed_temporal_unit.start_timestamp,
        delayed_temporal_unit.num_untrimmed_samples,
        delayed_temporal_unit.is_key_frame,
        absl::MakeConstSpan(delayed_temporal_unit.data)));
  }
  delayed_temporal_units_.clear();
  // Then finally, flush the current temporal unit.
  return PushSerializedTemporalUnit(temporal_unit.start_timestamp_,
                                    temporal_unit.num_untrimmed_samples_,
                                    is_key_frame, serialized_temporal_unit);
}

}  // namespace iamf_tools
