/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_PROBE_H_
#define CLI_PROBE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace iamf_tools {

// The `*Report` structs below mirror the wire-level fields of the
// corresponding OBUs, lightly decoded for readability. Conventions shared by
// all of them:
//
//   - Enum-like fields are reported as lower-case strings (e.g. "mix_gain",
//     "channel_based"). Values the spec marks as reserved are reported as
//     "reserved(N)" where N is the raw coded value. Each enum string is
//     accompanied by a sibling `*_raw` field carrying the raw coded value,
//     so programmatic consumers can branch on stable values instead of
//     display labels.
//   - Q7.8 fixed-point quantities are reported as both the raw coded
//     `int16_t` (`*_q7_8`) and the decoded `float` (raw / 256.0).
//   - Field names follow the IAMF specification
//     (https://aomediacodec.github.io/iamf/) unless noted otherwise.

// ---------- Codec Config ----------

/*!\brief Opus decoder config fields of a Codec Config OBU.
 *
 * Mirrors the `OpusDecoderConfig` syntax element, which itself mirrors the
 * Ogg Opus identification header.
 */
struct OpusDecoderConfigReport {
  uint8_t version;
  uint8_t output_channel_count;
  uint16_t pre_skip;
  uint32_t input_sample_rate;
  int16_t output_gain_q7_8;  // Raw signed Q7.8 gain.
  float output_gain;         // Decoded (raw / 256.0), in dB.
  uint8_t mapping_family;
};

/*!\brief LPCM decoder config fields of a Codec Config OBU. */
struct LpcmDecoderConfigReport {
  std::string sample_format;  // "little_endian", "big_endian", "reserved(N)".
  uint8_t sample_format_raw;
  uint8_t sample_size;   // Bits per sample.
  uint32_t sample_rate;  // Hz.
};

/*!\brief AAC-LC decoder config fields of a Codec Config OBU.
 *
 * Field names follow the embedded `DecoderConfigDescriptor` /
 * `AudioSpecificConfig` (ISO 14496-1 / 14496-3) syntax.
 */
struct AacDecoderConfigReport {
  uint8_t object_type_indication;
  uint8_t stream_type;
  bool upstream;
  bool reserved;
  uint32_t buffer_size_db;
  uint32_t max_bitrate;
  uint32_t average_bit_rate;
  uint8_t audio_object_type;
  std::string sample_frequency_index;  // e.g. "48000", "reserved(13)".
  uint8_t sample_frequency_index_raw;
  uint8_t channel_configuration;
};

/*!\brief One FLAC metadata block of a FLAC Codec Config OBU.
 *
 * The STREAMINFO fields below `is_stream_info` are only populated when
 * `is_stream_info` is true; other block types report only `block_type`.
 */
struct FlacMetaBlockReport {
  std::string block_type;  // e.g. "StreamInfo", "Padding", "reserved(7)".
  uint8_t block_type_raw = 0;
  bool is_stream_info = false;
  uint16_t minimum_block_size = 0;
  uint16_t maximum_block_size = 0;
  uint32_t minimum_frame_size = 0;
  uint32_t maximum_frame_size = 0;
  uint32_t sample_rate = 0;
  uint8_t number_of_channels = 0;
  uint8_t bits_per_sample = 0;
  uint64_t total_samples_in_stream = 0;
};

/*!\brief FLAC decoder config fields of a Codec Config OBU. */
struct FlacDecoderConfigReport {
  std::vector<FlacMetaBlockReport> metadata_blocks;
};

/*!\brief Summary of one Codec Config OBU. */
struct CodecConfigReport {
  uint32_t id;
  std::string codec_id;   // "Opus", "FLAC", "LPCM", "AAC LC".
  uint32_t codec_id_raw;  // The four-character code as a big-endian u32.
  uint32_t num_samples_per_frame;
  int16_t audio_roll_distance;
  uint32_t output_sample_rate;
  uint32_t input_sample_rate;
  // Bit depth used to measure loudness; not necessarily the coded sample
  // size (see `LpcmDecoderConfigReport::sample_size`).
  int bit_depth;

  // Exactly one of these is populated, matching `codec_id`.
  std::optional<OpusDecoderConfigReport> opus;
  std::optional<LpcmDecoderConfigReport> lpcm;
  std::optional<AacDecoderConfigReport> aac;
  std::optional<FlacDecoderConfigReport> flac;
};

// ---------- Audio Element ----------

/*!\brief One layer of a scalable channel audio config.
 *
 * Mirrors `ChannelAudioLayerConfig` in an Audio Element OBU. When
 * `loudspeaker_layout` is "Expanded", `expanded_loudspeaker_layout` carries
 * the resolved expanded layout name; it is nullopt otherwise.
 */
struct ChannelAudioLayerReport {
  std::string loudspeaker_layout;  // e.g. "Stereo", "5.1.4", "Expanded".
  uint8_t loudspeaker_layout_raw;
  std::optional<std::string> expanded_loudspeaker_layout;
  std::optional<uint8_t> expanded_loudspeaker_layout_raw;
  bool output_gain_is_present_flag;
  bool recon_gain_is_present_flag;
  uint8_t substream_count;
  uint8_t coupled_substream_count;
  uint8_t output_gain_flag;  // 6-bit mask of channels the gain applies to.
  int16_t output_gain_q7_8;  // Raw signed Q7.8 gain.
  float output_gain;         // Decoded (raw / 256.0), in dB.
};

/*!\brief Ambisonics mono config of a scene-based Audio Element OBU. */
struct AmbisonicsMonoReport {
  uint8_t output_channel_count;
  uint8_t substream_count;
  std::vector<uint8_t> channel_mapping;
};

/*!\brief Ambisonics projection config of a scene-based Audio Element OBU.
 *
 * `demixing_matrix` holds the raw signed 16-bit matrix coefficients in the
 * order they are coded in the OBU.
 */
struct AmbisonicsProjectionReport {
  uint8_t output_channel_count;
  uint8_t substream_count;
  uint8_t coupled_substream_count;
  std::vector<int16_t> demixing_matrix;
};

/*!\brief Ambisonics config of a scene-based Audio Element OBU.
 *
 * Exactly one of `mono` / `projection` is populated, matching `mode`.
 * `order` is derived from the config's output channel count: it is set to N
 * when that count equals (N + 1)^2, and is nullopt when the count is not a
 * perfect square.
 */
struct AmbisonicsReport {
  std::string mode;  // "mono", "projection", "reserved(N)".
  uint8_t mode_raw;
  std::optional<AmbisonicsMonoReport> mono;
  std::optional<AmbisonicsProjectionReport> projection;
  std::optional<int> order;
};

/*!\brief One parameter definition declared by an Audio Element OBU. */
struct AudioElementParamReport {
  std::string param_type;  // "mix_gain", "demixing", "recon_gain", etc.
  uint32_t param_type_raw;
  uint32_t parameter_id;
  uint32_t parameter_rate;
  uint8_t param_definition_mode;
  uint32_t duration;
  uint32_t constant_subblock_duration;
};

/*!\brief Summary of one Audio Element OBU.
 *
 * `scalable_layers` is populated for channel-based elements; `ambisonics`
 * is populated for scene-based elements. The convenience fields at the
 * bottom duplicate the top-most layer / ambisonics info so summary output
 * does not need to descend into the per-config structs.
 */
struct AudioElementReport {
  uint32_t id;
  std::string type;  // "channel_based", "scene_based", "object_based", ...
  uint8_t type_raw;
  uint8_t reserved;
  uint32_t codec_config_id;
  uint32_t num_substreams;
  std::vector<uint32_t> substream_ids;
  std::vector<AudioElementParamReport> params;

  // Channel-based:
  std::vector<ChannelAudioLayerReport> scalable_layers;

  // Scene-based:
  std::optional<AmbisonicsReport> ambisonics;

  // Convenience fields (top-most info).
  std::optional<std::string> channel_layout;
  // Raw coded `loudspeaker_layout` of the top-most layer: a stable
  // identifier for gating logic, immune to display-label rewording. When
  // the layout is "Expanded", `expanded_channel_layout_raw` carries the raw
  // expanded layout value.
  std::optional<uint8_t> channel_layout_raw;
  std::optional<uint8_t> expanded_channel_layout_raw;
  std::optional<int> ambisonics_order;
  std::optional<int> ambisonics_channels;
  // Decoded channel count of the element: for channel-based elements the
  // channels of the top-most layer (each coupled substream carries two
  // channels, each non-coupled one, summed across layers); for scene-based
  // elements the ambisonics output channel count. Unset for object-based
  // and reserved element types.
  std::optional<uint32_t> num_channels;
};

// ---------- Mix Presentation ----------

/*!\brief Common fields shared by every parameter definition. */
struct ParamDefinitionReport {
  uint32_t parameter_id;
  uint32_t parameter_rate;
  uint8_t param_definition_mode;
  uint32_t duration;
  uint32_t constant_subblock_duration;
};

/*!\brief A mix-gain parameter definition with its default gain.
 *
 * `default_mix_gain` is the decoded value of `default_mix_gain_q7_8`
 * (raw / 256.0), in dB.
 */
struct MixGainParamDefinitionReport {
  ParamDefinitionReport param_definition;
  int16_t default_mix_gain_q7_8;
  float default_mix_gain;
};

/*!\brief Rendering config of one audio element within a sub-mix. */
struct RenderingConfigReport {
  std::string
      headphones_rendering_mode;  // "stereo", "binaural_world_locked", ...
  uint8_t headphones_rendering_mode_raw;
  std::string
      binaural_filter_profile;  // "ambient", "direct", "reverberant", ...
  uint8_t binaural_filter_profile_raw;
};

/*!\brief One audio element referenced by a sub-mix. */
struct SubMixAudioElementReport {
  uint32_t audio_element_id;
  std::vector<std::string> localized_element_annotations;
  RenderingConfigReport rendering_config;
  MixGainParamDefinitionReport element_mix_gain;
};

/*!\brief One anchored loudness entry of a loudness info block.
 *
 * `anchored_loudness` is the decoded value of `anchored_loudness_q7_8`
 * (raw / 256.0), in LKFS.
 */
struct AnchoredLoudnessElementReport {
  std::string anchor_element;  // "unknown", "dialogue", "album", "reserved(N)".
  uint8_t anchor_element_raw;
  int16_t anchored_loudness_q7_8;
  float anchored_loudness;
};

/*!\brief Layout a sub-mix's loudness was measured on.
 *
 * `sound_system` is only populated when `layout_type` refers to an ITU-R
 * BS.2051 sound system ("loudspeakers_ss_convention"); it is nullopt for
 * other layout types such as binaural.
 */
struct LayoutReport {
  std::string layout_type;  // "loudspeakers_ss_convention", "binaural", ...
  uint8_t layout_type_raw;
  std::optional<std::string> sound_system;  // e.g. "7.1.4", "Stereo".
  std::optional<uint8_t> sound_system_raw;
};

/*!\brief Loudness information measured on one layout.
 *
 * `integrated_loudness` (LKFS), `digital_peak` (dBFS), and `true_peak`
 * (dBTP) are decoded from their Q7.8 coded forms (raw / 256.0).
 * `true_peak` is only populated when `true_peak_present` is true.
 */
struct LoudnessInfoReport {
  uint8_t info_type;  // Raw 8-bit mask.
  bool true_peak_present;
  bool anchored_loudness_present;
  bool has_layout_extension;  // Any layout-extension bit of `info_type` set.
  float integrated_loudness;
  float digital_peak;
  std::optional<float> true_peak;
  std::vector<AnchoredLoudnessElementReport> anchored_loudness_elements;
};

/*!\brief One (layout, loudness) pair of a sub-mix. */
struct MixPresentationLayoutReport {
  LayoutReport loudness_layout;
  LoudnessInfoReport loudness;
};

/*!\brief One sub-mix of a Mix Presentation OBU. */
struct SubMixReport {
  std::vector<SubMixAudioElementReport> audio_elements;
  MixGainParamDefinitionReport output_mix_gain;
  std::vector<MixPresentationLayoutReport> layouts;
};

/*!\brief One tag of a Mix Presentation Tags block. */
struct MixPresentationTagReport {
  std::string tag_name;
  std::string tag_value;
};

/*!\brief Summary of one Mix Presentation OBU.
 *
 * The convenience fields at the bottom are derived from the first sub-mix
 * and its first layout so summary output does not need to descend into
 * `sub_mixes`; consult `sub_mixes` for the full per-layout data.
 */
struct MixPresentationReport {
  uint32_t id;
  uint32_t count_label;
  // (language_code, localized_label) pairs.
  std::vector<std::pair<std::string, std::string>> annotations;
  uint32_t num_sub_mixes;
  std::vector<SubMixReport> sub_mixes;
  std::vector<MixPresentationTagReport> tags;  // Empty if absent.

  // Convenience summary derived from the first sub-mix / first layout.
  std::vector<std::string> layouts;
  std::optional<float> integrated_loudness_lkfs;
  std::optional<float> digital_peak_dbfs;
  std::optional<float> true_peak_dbfs;
};

// ---------- Temporal Unit Scan (opt-in) ----------

/*!\brief Mix-gain animation of one parameter subblock.
 *
 * The point values are mix gains in dB, reported as raw Q7.8 plus decoded
 * float pairs.
 */
struct MixGainAnimationReport {
  std::string animation_type;  // "step", "linear", "bezier", "reserved(N)".
  uint32_t animation_type_raw = 0;
  int16_t start_point_value_q7_8 = 0;
  float start_point_value = 0.0f;
  std::optional<int16_t> end_point_value_q7_8;
  std::optional<float> end_point_value;
  std::optional<int16_t> control_point_value_q7_8;
  std::optional<float> control_point_value;
  std::optional<uint8_t> control_point_relative_time;  // Raw unsigned Q0.8.
};

/*!\brief Demixing info of one parameter subblock. */
struct DemixingInfoReport {
  std::string dmixp_mode;  // "mode1", "mode2", ..., "reserved(N)".
  uint8_t dmixp_mode_raw = 0;
  uint8_t reserved = 0;
};

/*!\brief Recon gain values for one layer of a scalable channel config. */
struct ReconGainForLayerReport {
  uint8_t layer_index;
  uint32_t recon_gain_flag;         // ULEB128-coded channel bitmask.
  std::vector<uint8_t> recon_gain;  // Values for channels with the flag set.
};

/*!\brief Recon gain info of one parameter subblock. */
struct ReconGainInfoReport {
  std::vector<ReconGainForLayerReport> layers;
};

/*!\brief One subblock of a Parameter Block OBU.
 *
 * At most one of `mix_gain` / `demixing` / `recon_gain` is populated,
 * matching the parameter type of the enclosing block.
 */
struct ParameterSubblockReport {
  // Resolved from the block / constant / variable duration rules.
  uint32_t subblock_duration = 0;
  std::optional<MixGainAnimationReport> mix_gain;
  std::optional<DemixingInfoReport> demixing;
  std::optional<ReconGainInfoReport> recon_gain;
};

/*!\brief Summary of one Parameter Block OBU seen during the scan. */
struct ParameterBlockReport {
  uint32_t parameter_id = 0;
  std::string param_type;  // Same vocabulary as `AudioElementParamReport`.
  // Raw coded parameter definition type; 0 with `param_type` "unknown" when
  // the parameter id was not declared in the descriptors.
  uint32_t param_type_raw = 0;
  uint64_t start_timestamp = 0;  // In ticks of the parameter rate.
  uint64_t end_timestamp = 0;
  uint32_t duration = 0;
  uint32_t constant_subblock_duration = 0;
  std::vector<ParameterSubblockReport> subblocks;
};

/*!\brief Per-substream audio frame totals accumulated by the scan. */
struct AudioFrameSummaryReport {
  uint32_t substream_id = 0;
  uint32_t codec_config_id = 0;
  uint32_t frame_count = 0;
  // Sum of `num_samples_per_frame` across the substream's frames.
  uint64_t total_samples = 0;
};

/*!\brief Locator for one temporal unit, emitted by the scan.
 *
 * Enables a binary search from a target sample position to the byte offset
 * of the containing temporal unit (TU), e.g. to seek within a stream.
 *
 * `byte_offset_from_scan_start` is measured from the byte immediately after
 * the last descriptor OBU (i.e. the first temporal unit byte); the absolute
 * input offset is `ProbeReport::descriptor_bytes_consumed +
 * byte_offset_from_scan_start`. `start_timestamp` is the raw sample index of
 * the TU's first sample. `num_samples` is the maximum substream frame size
 * across the audio frames present in the TU (all substreams share a frame
 * size in well-formed streams).
 *
 * The trim fields give the sample counts to drop from the decoded output of
 * this TU. Both are read from the first audio frame OBU header seen in the
 * TU; within a conformant IAMF stream every frame in a TU carries identical
 * trim values. For Opus the sum of `samples_to_trim_at_start` across the
 * leading TUs equals the codec config's `pre_skip`. FLAC and LPCM streams
 * carry zeros.
 */
struct TemporalUnitEntry {
  uint64_t start_timestamp = 0;
  uint64_t num_samples = 0;
  size_t byte_offset_from_scan_start = 0;
  uint32_t samples_to_trim_at_start = 0;
  uint32_t samples_to_trim_at_end = 0;
};

/*!\brief Result of the opt-in temporal-unit scan.
 *
 * Populated in `ProbeReport::temporal_unit_scan` when
 * `ProbeOptions::scan_mode` requests a scan. The scan is resilient to
 * damaged input: OBUs that fail to parse are counted in the
 * `*_parse_errors` fields and skipped, so callers can tell a healthy file
 * from a mangled one, and `stopped_reason` records why the scan ended.
 */
struct TemporalUnitScanReport {
  uint32_t temporal_unit_count = 0;
  uint32_t audio_frame_count = 0;
  uint32_t parameter_block_count = 0;
  uint32_t temporal_delimiter_count = 0;
  // Counts of OBUs whose body failed to parse (or whose parameter id was not
  // declared in the descriptors) and that the scan therefore skipped.
  uint32_t audio_frame_parse_errors = 0;
  uint32_t parameter_block_parse_errors = 0;
  std::vector<AudioFrameSummaryReport> audio_frames_by_substream;
  // In stream order. Both stay empty unless `ProbeOptions::scan_mode` is
  // `ScanMode::kScanFull`.
  std::vector<ParameterBlockReport> parameter_blocks;
  std::vector<TemporalUnitEntry> temporal_units;
  std::optional<uint64_t> total_samples;       // Max substream total.
  std::optional<double> duration_seconds;      // Derived from total_samples.
  std::optional<uint32_t> output_sample_rate;  // Rate used for duration.
  size_t bytes_consumed = 0;
  // Why the scan ended:
  // "eof":                  no bytes remained when starting the next OBU.
  // "truncated":            bytes existed but were insufficient to complete
  //                         an OBU.
  // "malformed":            bytes formed a syntactically invalid OBU header,
  //                         or a buffer operation failed unexpectedly during
  //                         resync.
  // "next_ia_sequence":     hit a canonical IA Sequence Header (rewound so
  //                         the caller can start a fresh scan there).
  // "scan_budget_exceeded": `ProbeOptions::max_scan_obu_iterations` was hit
  //                         before reaching the end of the input.
  // "cancelled":            `ProbeOptions::should_continue` returned false.
  std::string stopped_reason;
};

/*!\brief Structured summary of an IA sequence, returned by `Probe`. */
struct ProbeReport {
  std::string primary_profile;  // "simple", "base", "base_enhanced", ...
  uint8_t primary_profile_raw = 0;
  std::string additional_profile;  // Same vocabulary as `primary_profile`.
  uint8_t additional_profile_raw = 0;
  std::vector<CodecConfigReport> codec_configs;
  std::vector<AudioElementReport> audio_elements;
  std::vector<MixPresentationReport> mix_presentations;
  // Bytes of `data` occupied by the descriptor OBUs; temporal units (if
  // any) start at this offset.
  size_t descriptor_bytes_consumed = 0;
  // Descriptors-only duration estimate, derived from codec config metadata
  // (currently: FLAC STREAMINFO `total_samples_in_stream`) without any
  // temporal-unit scan. Codec-config-derived and therefore only as
  // trustworthy as the encoder that wrote it — distinct from the
  // scan-measured `TemporalUnitScanReport::duration_seconds`, which counts
  // actual audio frames. Unset when no codec config carries a usable total.
  std::optional<uint64_t> descriptor_total_samples;
  std::optional<double> descriptor_duration_seconds;
  // Populated only when `ProbeOptions::scan_mode` requests a scan.
  std::optional<TemporalUnitScanReport> temporal_unit_scan;
};

/*!\brief How far past the descriptor OBUs `Probe` should look. */
enum class ScanMode {
  // Parse descriptor OBUs only; any trailing temporal-unit bytes are
  // ignored and `ProbeReport::temporal_unit_scan` stays unset.
  kDescriptorsOnly,
  // Walk temporal units after the descriptor OBUs, accumulating OBU counts,
  // per-substream sample totals, `total_samples`, and `duration_seconds`.
  // Parameter-block bodies are skipped without parsing;
  // `parameter_block_parse_errors` then only counts blocks whose parameter
  // id could not be peeked or was not declared in the descriptors. The
  // report stays O(#substreams) instead of O(#TUs): use this for
  // duration-only probes of large files.
  kScanCounts,
  // Everything `kScanCounts` collects, plus the per-TU index
  // (`temporal_units`) and parsed parameter-block contents
  // (`parameter_blocks`, with mix-gain animations, demixing info, and recon
  // gain).
  kScanFull,
};

/*!\brief Progress snapshot passed to `ProbeOptions::should_continue`. */
struct ScanProgress {
  // Scan bytes consumed so far (measured from the first temporal-unit
  // byte, like `TemporalUnitScanReport::bytes_consumed`).
  size_t bytes_consumed = 0;
  // Temporal units finalized so far.
  uint32_t temporal_unit_count = 0;
};

/*!\brief Options controlling what `Probe` extracts. */
struct ProbeOptions {
  // How far past the descriptor OBUs to look; see `ScanMode`.
  ScanMode scan_mode = ScanMode::kDescriptorsOnly;
  // Optional cancellation/progress hook, consulted once per OBU during the
  // temporal-unit scan (descriptor parsing is fast and not interruptible).
  // Return false to stop the scan: the partial results accumulated so far
  // remain valid, matching the scan's best-effort semantics, and
  // `stopped_reason` is set to "cancelled".
  std::function<bool(const ScanProgress&)> should_continue;
  // Ceiling on scan OBU iterations. The scan is naturally bounded by the
  // input size (every iteration consumes bytes); this additionally caps
  // latency on adversarial inputs packed with tiny reserved OBUs. The
  // default is far beyond any legitimate IAMF file. When hit,
  // `stopped_reason` is "scan_budget_exceeded".
  uint64_t max_scan_obu_iterations = 10'000'000;
};

/*!\brief Parses descriptor OBUs from `data` and returns a structured summary.
 *
 * Unlike a decoder, `Probe` touches no audio: it parses the descriptor OBUs
 * (and, optionally, walks the temporal units) to summarize what the sequence
 * contains. `data` should contain, at minimum, all descriptor OBUs of an IA
 * sequence. Trailing temporal unit bytes are permitted and ignored unless
 * `options.scan_mode` requests a scan, in which case they are walked and
 * summarized in `ProbeReport::temporal_unit_scan`.
 *
 * \param data Bytes of an IA sequence, starting at the IA Sequence Header
 *        OBU.
 * \param options Controls the optional temporal-unit scan.
 * \return A `ProbeReport` summarizing the sequence. Returns
 *         `kResourceExhausted` if `data` ends before the descriptor OBUs are
 *         complete (the input may be valid; retry with more bytes), or
 *         `kInvalidArgument` if the bytes are not a valid IAMF descriptor
 *         sequence.
 */
absl::StatusOr<ProbeReport> Probe(absl::Span<const uint8_t> data,
                                  ProbeOptions options = {});

/*!\brief Probes an IAMF file on disk.
 *
 * Equivalent to reading all of `path` and calling `Probe`, but reads
 * incrementally through a fixed-size window: a descriptors-only probe
 * touches just the descriptor bytes, and the optional temporal-unit scan
 * streams the rest of the file instead of loading it into memory. Use this
 * to probe files whose size dwarfs their descriptors.
 *
 * \param path Path of a file containing an IA sequence, starting at the IA
 *        Sequence Header OBU.
 * \param options Controls the optional temporal-unit scan.
 * \return A `ProbeReport` summarizing the sequence. Returns `kNotFound` if
 *         the file cannot be opened; otherwise errors as for `Probe`, except
 *         that truncated descriptors report `kInvalidArgument` (the whole
 *         file is visible, so more bytes cannot arrive).
 */
absl::StatusOr<ProbeReport> ProbeFile(const std::string& path,
                                      ProbeOptions options = {});

}  // namespace iamf_tools

#endif  // CLI_PROBE_H_
