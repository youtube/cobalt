/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/absl_log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/api/internal_utils/internal_utils.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/include/iamf_tools/iamf_decoder_factory.h"
#include "iamf/include/iamf_tools/iamf_decoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

// Control input and output files.
ABSL_FLAG(std::string, input_filename, "", "Filename of the input IAMF file.");
ABSL_FLAG(std::string, output_filename, "", "Filename of the output wav file.");

// Control properties of the input and output files.
ABSL_FLAG(int, block_size, 1024,
          "Size in bytes of blocks to feed the decoder.");
ABSL_FLAG(std::optional<iamf_tools::api::OutputSampleType>, output_sample_type,
          std::nullopt,
          "Type of output. Currently `sle16` and `sle32` are supported. "
          "If omitted, the decoder will select one based on the input stream.");
ABSL_FLAG(iamf_tools::api::OutputLayout, output_layout,
          iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0,
          "Output layout.");
ABSL_FLAG(
    std::optional<uint32_t>, mix_id, std::nullopt,
    "Mix ID to decode. If omitted, one is selected automatically, depending on "
    "the requested layout and mix presentations in the file.");

namespace iamf_tools {
namespace api {

namespace {
inline static constexpr auto kApiOutputLayoutAndFlagString = []() {
  using enum iamf_tools::api::OutputLayout;
  return std::to_array<
      std::pair<iamf_tools::api::OutputLayout, absl::string_view>>({
      {kItu2051_SoundSystemA_0_2_0, "2.0"},
      {kItu2051_SoundSystemB_0_5_0, "5.1"},
      {kItu2051_SoundSystemC_2_5_0, "5.1.2"},
      {kItu2051_SoundSystemD_4_5_0, "5.1.4"},
      {kItu2051_SoundSystemE_4_5_1, "5+4+1"},
      {kItu2051_SoundSystemF_3_7_0, "3+7+0"},
      {kItu2051_SoundSystemG_4_9_0, "9.1.4"},
      {kItu2051_SoundSystemH_9_10_3, "22.0"},
      {kItu2051_SoundSystemI_0_7_0, "7.1"},
      {kItu2051_SoundSystemJ_4_7_0, "7.1.4"},
      {kIAMF_SoundSystemExtension_2_7_0, "7.1.2"},
      {kIAMF_SoundSystemExtension_2_3_0, "3.1.2"},
      {kIAMF_SoundSystemExtension_0_1_0, "1.0"},
      {kIAMF_SoundSystemExtension_6_9_0, "9.1.6"},
      {kIAMF_Binaural, "Binaural"},
  });
}();

}  // namespace

bool AbslParseFlag(absl::string_view string_output_layout,
                   iamf_tools::api::OutputLayout* api_output_layout,
                   std::string* error) {
  if (string_output_layout.empty()) {
    *error = "No output layout specified.";
    return false;
  }

  static const auto kFlagStringToApiOutputLayout =
      BuildStaticMapFromInvertedPairs(kApiOutputLayoutAndFlagString);
  auto status =
      CopyFromMap(*kFlagStringToApiOutputLayout, string_output_layout,
                  "`OutputLayout` for flag string", *api_output_layout);
  if (!status.ok()) {
    *error = status.message();
    return false;
  }

  return true;
}

std::string AbslUnparseFlag(iamf_tools::api::OutputLayout api_output_layout) {
  static const auto kApiOutputLayoutToFlagString =
      BuildStaticMapFromPairs(kApiOutputLayoutAndFlagString);

  absl::StatusOr<absl::string_view> string_output_layout =
      LookupInMap(*kApiOutputLayoutToFlagString, api_output_layout,
                  "Flag string for `OutputLayout`");

  if (!string_output_layout.ok()) {
    return "Unsupported output layout.";
  }
  return std::string(*string_output_layout);
}

bool AbslParseFlag(absl::string_view string_sample_type,
                   iamf_tools::api::OutputSampleType* output_sample_type,
                   std::string* error) {
  // Flags named based on their wav file format name.
  using enum iamf_tools::api::OutputSampleType;
  if (string_sample_type.empty()) {
    *error = "No output type specified.";
    return false;
  } else if (string_sample_type == "sle16") {
    *output_sample_type = kInt16LittleEndian;
  } else if (string_sample_type == "sle32") {
    *output_sample_type = kInt32LittleEndian;
  } else {
    *error = "Unsupported output sample type.";
    return false;
  }
  return true;
}

std::string AbslUnparseFlag(
    iamf_tools::api::OutputSampleType string_sample_type) {
  switch (string_sample_type) {
    using enum iamf_tools::api::OutputSampleType;
    case kInt16LittleEndian:
      return "sle16";
    case kInt32LittleEndian:
      return "sle32";
    default:
      return "Unsupported output sample type.";
  }
}

void LogSelectedMix(std::optional<uint32_t> requested_mix_presentation_id,
                    const IamfDecoderInterface& decoder) {
  iamf_tools::api::SelectedMix selected_mix;
  auto status = decoder.GetOutputMix(selected_mix);
  if (!status.ok()) {
    ABSL_LOG(FATAL) << "Failed to get output mix: " << status;
  }
  // Sometimes the user selected mix is not present, or available. Log a
  // warning, but proceed with the mix that was selected.
  if (requested_mix_presentation_id.has_value() &&
      selected_mix.mix_presentation_id != *requested_mix_presentation_id) {
    ABSL_LOG(WARNING) << "Failed to decode requested mix presentation ID. "
                         "Falling back to a different mix presentation ID.";
  }
  ABSL_LOG(INFO) << "Decoding Mix Presentation ID: "
                 << selected_mix.mix_presentation_id << ".";
}

}  // namespace api
}  // namespace iamf_tools

using ::iamf_tools::WavWriter;
using ::iamf_tools::api::IamfDecoderFactory;
using ::iamf_tools::api::IamfDecoderInterface;

// Read a chunk of data from the input stream into a backing buffer. Return a
// span to the valid portion of the buffer.
absl::Span<const uint8_t> ReadChunk(std::ifstream& input_stream,
                                    std::vector<uint8_t>& backing_buffer) {
  input_stream.read(reinterpret_cast<char*>(backing_buffer.data()),
                    backing_buffer.size());

  // At EOF, we may have read less than the full block size. Return a span of
  // the valid portion of the buffer.
  return absl::MakeConstSpan(backing_buffer).first(input_stream.gcount());
}

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  const auto input_filename = absl::GetFlag(FLAGS_input_filename);
  const auto output_filename = absl::GetFlag(FLAGS_output_filename);
  const int block_size = absl::GetFlag(FLAGS_block_size);
  const auto output_layout = absl::GetFlag(FLAGS_output_layout);
  const auto requested_mix_presentation_id = absl::GetFlag(FLAGS_mix_id);
  const auto output_sample_type = absl::GetFlag(FLAGS_output_sample_type);

  if (input_filename.empty() || output_filename.empty()) {
    ABSL_LOG(ERROR)
        << "--input_filename and --output_filename must be specified.";
    return EXIT_FAILURE;
  }

  IamfDecoderFactory::Settings settings = {
      .requested_mix = {.mix_presentation_id = requested_mix_presentation_id,
                        .output_layout = output_layout}};

  // Force the output sample type when requested.
  if (output_sample_type.has_value()) {
    settings.requested_output_sample_type = *output_sample_type;
  }
  ABSL_LOG(INFO) << "Creating decoder.";
  std::unique_ptr<IamfDecoderInterface> decoder =
      IamfDecoderFactory::Create(settings);
  if (decoder == nullptr) {
    ABSL_LOG(FATAL) << "Failed to create decoder.";
  }

  // Source file to stream to the decoder.
  std::ifstream input_stream(input_filename, std::ios::binary | std::ios::in);
  if (!input_stream.is_open() || input_stream.fail()) {
    ABSL_LOG(FATAL) << "Failed to open input file: " << input_filename;
  }
  // Buffer to feed to the decoder. Read in chunks of length `block_size` from
  // the source file.
  std::vector<uint8_t> input_buffer(block_size);

  bool got_descriptors = false;
  int64_t num_temporal_units_processed_for_logging = 0;
  // Some state that will be configured after the descriptors are processed.
  std::unique_ptr<WavWriter> wav_writer;
  // Reuse the same output buffer between calls.
  std::vector<uint8_t> reusable_sample_buffer;

  while (input_stream.good()) {
    const auto next_chunk = ReadChunk(input_stream, input_buffer);
    ABSL_LOG_EVERY_N_SEC(INFO, 5)
        << "Decoding. " << next_chunk.size() << " bytes.";

    iamf_tools::api::IamfStatus decode_status =
        decoder->Decode(next_chunk.data(), next_chunk.size());
    if (!decode_status.ok()) {
      ABSL_LOG(FATAL) << "Failed to decode: " << decode_status;
    }
    if (input_stream.eof()) {
      ABSL_LOG(INFO) << "Reached EOF.";
      auto status = decoder->SignalEndOfDecoding();
      if (!status.ok()) {
        ABSL_LOG(FATAL)
            << "Failed signalling end of decoding. Some data may have "
               "been lost. Status: "
            << status;
      }
      // Dump one last time, the file could have been shorter than the block
      // size, or there was just some remaining data to be flushed out.
    }

    // Catch the first time descriptors are processed.
    if (!got_descriptors && decoder->IsDescriptorProcessingComplete()) {
      ABSL_LOG(INFO) << "Got descriptors.";
      got_descriptors = true;
      // Configure the wav writer and reusable sample buffer.
      auto status = SetupAfterDescriptors(*decoder, output_filename, wav_writer,
                                          reusable_sample_buffer);
      if (!status.ok()) {
        ABSL_LOG(FATAL) << "Failed to setup after descriptors: " << status;
      }
      LogSelectedMix(requested_mix_presentation_id, *decoder);
    }
    if (got_descriptors) {
      // Should only be called after descriptors are processed. Descriptors may
      // be processed in this iteration or a subsequent one; we also want to
      // support the case where we get the entire file at once, and the
      // descriptors and temporal units are all available in this iteration.
      int num_temporal_units_processed;
      if (wav_writer == nullptr) {
        ABSL_LOG(FATAL) << "Wav writer is null";
      }
      auto status = DumpPendingTemporalUnitsToWav(
          *decoder, reusable_sample_buffer, *wav_writer,
          num_temporal_units_processed);
      if (!status.ok()) {
        ABSL_LOG(FATAL) << "Failed to dump pending temporal units to wav: "
                        << status;
      }
      num_temporal_units_processed_for_logging += num_temporal_units_processed;
    }
  }

  ABSL_LOG(INFO) << "Decoded " << num_temporal_units_processed_for_logging
                 << " temporal units.";

  return EXIT_SUCCESS;
}
