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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "iamf/cli/probe.h"
#include "iamf/cli/probe_json.h"

ABSL_FLAG(std::string, input_filename, "", "Filename of the input IAMF file.");
ABSL_FLAG(std::string, format, "text", "Output format: json | text.");
ABSL_FLAG(std::string, scan, "",
          "Temporal-unit scan mode: counts | full. \"counts\" accumulates "
          "OBU counts, per-substream totals, and duration only; much faster "
          "on large files. \"full\" additionally records the per-TU index "
          "and parameter-block contents. Empty (the default) skips the "
          "scan.");
ABSL_FLAG(bool, duration, false, "Shorthand for --scan=counts.");

namespace {

// ---------- Human-readable text emitter ----------

std::string FormatText(const iamf_tools::ProbeReport& r) {
  std::string out;
  absl::StrAppend(&out, "IA Sequence Header:\n");
  absl::StrAppend(&out, "  primary_profile:    ", r.primary_profile, "\n");
  absl::StrAppend(&out, "  additional_profile: ", r.additional_profile, "\n");
  absl::StrAppend(&out, "  descriptor_bytes:   ", r.descriptor_bytes_consumed,
                  "\n");
  if (r.descriptor_duration_seconds.has_value()) {
    absl::StrAppend(&out, "  est_duration_secs:  ",
                    absl::StrFormat("%.6f", *r.descriptor_duration_seconds),
                    " (from codec config metadata)\n");
  }
  absl::StrAppend(&out, "\n");

  absl::StrAppend(&out, "Codec Configs (", r.codec_configs.size(), "):\n");
  for (const auto& c : r.codec_configs) {
    absl::StrAppend(
        &out, "  - id=", c.id, " codec=", c.codec_id,
        " frame=", c.num_samples_per_frame, " roll=", c.audio_roll_distance,
        " in_sr=", c.input_sample_rate, " out_sr=", c.output_sample_rate,
        " bit_depth=", c.bit_depth, "\n");
    if (c.opus.has_value()) {
      const auto& o = *c.opus;
      absl::StrAppend(&out, "    opus: version=", o.version,
                      " channels=", o.output_channel_count,
                      " pre_skip=", o.pre_skip, " in_sr=", o.input_sample_rate,
                      " out_gain=", o.output_gain,
                      " mapping_family=", o.mapping_family, "\n");
    }
    if (c.lpcm.has_value()) {
      const auto& l = *c.lpcm;
      absl::StrAppend(&out, "    lpcm: fmt=", l.sample_format,
                      " size=", l.sample_size, " rate=", l.sample_rate, "\n");
    }
    if (c.aac.has_value()) {
      const auto& a = *c.aac;
      absl::StrAppend(&out, "    aac: obj_type=", a.audio_object_type,
                      " sf=", a.sample_frequency_index,
                      " ch=", a.channel_configuration,
                      " avg_bitrate=", a.average_bit_rate, "\n");
    }
    if (c.flac.has_value()) {
      absl::StrAppend(&out, "    flac: ", c.flac->metadata_blocks.size(),
                      " metadata_blocks\n");
      for (const auto& b : c.flac->metadata_blocks) {
        absl::StrAppend(&out, "      - ", b.block_type);
        if (b.is_stream_info) {
          absl::StrAppend(&out, " (sr=", b.sample_rate,
                          " ch=", b.number_of_channels,
                          " bps=", b.bits_per_sample, ")");
        }
        absl::StrAppend(&out, "\n");
      }
    }
  }

  absl::StrAppend(&out, "\nAudio Elements (", r.audio_elements.size(), "):\n");
  for (const auto& a : r.audio_elements) {
    absl::StrAppend(&out, "  - id=", a.id, " type=", a.type,
                    " codec_cfg=", a.codec_config_id,
                    " substreams=", a.num_substreams);
    if (a.num_channels.has_value()) {
      absl::StrAppend(&out, " channels=", *a.num_channels);
    }
    if (!a.substream_ids.empty()) {
      absl::StrAppend(&out, " ids=[", absl::StrJoin(a.substream_ids, ","), "]");
    }
    absl::StrAppend(&out, "\n");
    for (const auto& p : a.params) {
      absl::StrAppend(&out, "    param: type=", p.param_type,
                      " id=", p.parameter_id, " rate=", p.parameter_rate,
                      " duration=", p.duration, "\n");
    }
    for (const auto& l : a.scalable_layers) {
      absl::StrAppend(&out, "    layer: ", l.loudspeaker_layout,
                      " substreams=", static_cast<int>(l.substream_count),
                      " coupled=", static_cast<int>(l.coupled_substream_count));
      if (l.expanded_loudspeaker_layout.has_value()) {
        absl::StrAppend(&out, " expanded=", *l.expanded_loudspeaker_layout);
      }
      if (l.output_gain_is_present_flag) {
        absl::StrAppend(&out, " output_gain=", l.output_gain,
                        " flag=", static_cast<int>(l.output_gain_flag));
      }
      if (l.recon_gain_is_present_flag) {
        absl::StrAppend(&out, " recon_gain=yes");
      }
      absl::StrAppend(&out, "\n");
    }
    if (a.ambisonics.has_value()) {
      const auto& amb = *a.ambisonics;
      absl::StrAppend(&out, "    ambisonics: mode=", amb.mode);
      if (amb.order.has_value()) {
        absl::StrAppend(&out, " order=", *amb.order);
      }
      if (amb.mono.has_value()) {
        absl::StrAppend(
            &out,
            " channels=", static_cast<int>(amb.mono->output_channel_count),
            " substreams=", static_cast<int>(amb.mono->substream_count));
      } else if (amb.projection.has_value()) {
        absl::StrAppend(
            &out, " channels=",
            static_cast<int>(amb.projection->output_channel_count),
            " substreams=", static_cast<int>(amb.projection->substream_count),
            " coupled=",
            static_cast<int>(amb.projection->coupled_substream_count));
      }
      absl::StrAppend(&out, "\n");
    }
  }

  absl::StrAppend(&out, "\nMix Presentations (", r.mix_presentations.size(),
                  "):\n");
  for (const auto& m : r.mix_presentations) {
    absl::StrAppend(&out, "  - id=", m.id, " sub_mixes=", m.num_sub_mixes,
                    " count_label=", m.count_label, "\n");
    for (const auto& [lang, label] : m.annotations) {
      absl::StrAppend(&out, "    annotation: ", lang, " = \"", label, "\"\n");
    }
    for (size_t si = 0; si < m.sub_mixes.size(); ++si) {
      const auto& s = m.sub_mixes[si];
      absl::StrAppend(&out, "    sub_mix[", si,
                      "]: audio_elements=", s.audio_elements.size(),
                      " layouts=", s.layouts.size(), "\n");
      for (const auto& ae : s.audio_elements) {
        absl::StrAppend(
            &out, "      ae_id=", ae.audio_element_id,
            " headphones=", ae.rendering_config.headphones_rendering_mode,
            " filter=", ae.rendering_config.binaural_filter_profile,
            " mix_gain=", ae.element_mix_gain.default_mix_gain, "\n");
      }
      for (const auto& l : s.layouts) {
        const std::string layout_name = l.loudness_layout.sound_system.value_or(
            l.loudness_layout.layout_type);
        absl::StrAppend(
            &out, "      layout: ", layout_name,
            " loudness: integrated=", l.loudness.integrated_loudness,
            " peak=", l.loudness.digital_peak);
        if (l.loudness.true_peak.has_value()) {
          absl::StrAppend(&out, " true_peak=", *l.loudness.true_peak);
        }
        absl::StrAppend(&out, " info_type=0x",
                        absl::StrFormat("%02x", l.loudness.info_type), "\n");
      }
    }
    for (const auto& t : m.tags) {
      absl::StrAppend(&out, "    tag: ", t.tag_name, " = ", t.tag_value, "\n");
    }
  }

  if (r.temporal_unit_scan.has_value()) {
    const auto& s = *r.temporal_unit_scan;
    absl::StrAppend(&out, "\nTemporal Units:\n");
    absl::StrAppend(&out, "  count:             ", s.temporal_unit_count, "\n");
    absl::StrAppend(&out, "  audio_frames:      ", s.audio_frame_count, "\n");
    absl::StrAppend(&out, "  parameter_blocks:  ", s.parameter_block_count,
                    "\n");
    absl::StrAppend(&out, "  delimiters:        ", s.temporal_delimiter_count,
                    "\n");
    if (s.audio_frame_parse_errors > 0 || s.parameter_block_parse_errors > 0) {
      absl::StrAppend(
          &out, "  parse_errors:      frames=", s.audio_frame_parse_errors,
          " parameter_blocks=", s.parameter_block_parse_errors, "\n");
    }
    absl::StrAppend(&out, "  bytes_consumed:    ", s.bytes_consumed, "\n");
    absl::StrAppend(&out, "  stopped_reason:    ", s.stopped_reason, "\n");
    if (s.total_samples.has_value()) {
      absl::StrAppend(&out, "  total_samples:     ", *s.total_samples, "\n");
    }
    if (s.duration_seconds.has_value()) {
      absl::StrAppend(&out, "  duration_seconds:  ",
                      absl::StrFormat("%.6f", *s.duration_seconds), "\n");
    }
    for (const auto& f : s.audio_frames_by_substream) {
      absl::StrAppend(&out, "  substream id=", f.substream_id,
                      " codec_cfg=", f.codec_config_id,
                      " frames=", f.frame_count, " samples=", f.total_samples,
                      "\n");
    }
    for (const auto& b : s.parameter_blocks) {
      absl::StrAppend(&out, "  block id=", b.parameter_id,
                      " type=", b.param_type, " ts=[", b.start_timestamp, ",",
                      b.end_timestamp, ")", " dur=", b.duration,
                      " subblocks=", b.subblocks.size(), "\n");
      for (size_t i = 0; i < b.subblocks.size(); ++i) {
        const auto& sub = b.subblocks[i];
        absl::StrAppend(&out, "    sub[", i, "] dur=", sub.subblock_duration);
        if (sub.mix_gain.has_value()) {
          const auto& m = *sub.mix_gain;
          absl::StrAppend(&out, " mix_gain=", m.animation_type,
                          " start=", m.start_point_value);
          if (m.end_point_value.has_value()) {
            absl::StrAppend(&out, " end=", *m.end_point_value);
          }
          if (m.control_point_value.has_value()) {
            absl::StrAppend(&out, " ctrl=", *m.control_point_value, "@",
                            *m.control_point_relative_time);
          }
        }
        if (sub.demixing.has_value()) {
          absl::StrAppend(&out, " demixing=", sub.demixing->dmixp_mode);
        }
        if (sub.recon_gain.has_value()) {
          absl::StrAppend(&out,
                          " recon_gain_layers=", sub.recon_gain->layers.size());
        }
        absl::StrAppend(&out, "\n");
      }
    }
  }
  return out;
}

// Returns a usage hint for input the probe rejected, based on its leading
// bytes, or an empty string when no common mistake is recognized.
std::string SniffHint(absl::Span<const uint8_t> head) {
  // ISO BMFF: a 32-bit box size followed by "ftyp". IAMF usually travels
  // inside MP4, so this is the most likely real-world mix-up.
  if (head.size() >= 8 && head[4] == 'f' && head[5] == 't' && head[6] == 'y' &&
      head[7] == 'p') {
    return "; this looks like an MP4 file - extract the IAMF track first "
           "(e.g. with MP4Box or ffmpeg)";
  }
  // A leading temporal-unit OBU (types 3..23: parameter block, temporal
  // delimiter, audio frame) means the stream starts mid-sequence.
  if (!head.empty()) {
    const uint8_t obu_type = head[0] >> 3;
    if (obu_type >= 3 && obu_type <= 23) {
      return "; the input appears to start mid-stream (a temporal-unit OBU "
             "comes first; descriptor OBUs, starting with the IA Sequence "
             "Header, are required)";
    }
  }
  return "";
}

// Reads the first bytes of `path` for `SniffHint`. Best-effort: returns
// empty on any I/O problem.
std::vector<uint8_t> ReadHead(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::vector<uint8_t> head(8);
  in.read(reinterpret_cast<char*>(head.data()),
          static_cast<std::streamsize>(head.size()));
  head.resize(static_cast<size_t>(in.gcount()));
  return head;
}

constexpr char kUsage[] =
    "Probes descriptor OBUs of an IAMF file and prints a summary without "
    "decoding any audio.\n\n"
    "  probe_main --input_filename=<input.iamf>\n"
    "  ... | probe_main --input_filename=-          (read the stream from "
    "stdin)\n\n"
    "Use --format=json|text to choose the output format, or "
    "--scan=counts|full to also walk temporal units (per-substream totals "
    "and duration; \"full\" adds parameter-block contents and a per-TU "
    "index). See docs/iamf_probe_main.md.";

}  // namespace

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(kUsage);
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  // Suppress the OBU classes' verbose info-level prints; users only want the
  // probe output.
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kWarning);

  std::string input_filename = absl::GetFlag(FLAGS_input_filename);
  if (input_filename.empty()) {
    std::fputs(
        "error: no input; pass a filename (or - for stdin), e.g. "
        "probe_main --input_filename=input.iamf\n",
        stderr);
    return EXIT_FAILURE;
  }
  iamf_tools::ProbeOptions options;
  const std::string scan = absl::GetFlag(FLAGS_scan);
  if (scan == "full") {
    options.scan_mode = iamf_tools::ScanMode::kScanFull;
  } else if (scan == "counts") {
    options.scan_mode = iamf_tools::ScanMode::kScanCounts;
  } else if (!scan.empty()) {
    std::fprintf(stderr, "error: unknown --scan=%s (counts|full)\n",
                 scan.c_str());
    return EXIT_FAILURE;
  }
  if (absl::GetFlag(FLAGS_duration) &&
      options.scan_mode == iamf_tools::ScanMode::kDescriptorsOnly) {
    options.scan_mode = iamf_tools::ScanMode::kScanCounts;
  }
  absl::StatusOr<iamf_tools::ProbeReport> report_or;
  std::vector<uint8_t> head;
  if (input_filename == "-") {
    // Stdin is not seekable, so slurp it and probe the buffer. Stdin is
    // assumed to be in binary mode, which holds on POSIX; a Windows port
    // would need to switch it explicitly.
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(std::cin)),
                               std::istreambuf_iterator<char>());
    if (std::cin.bad()) {
      // iostreams do not reliably set errno, so no strerror here.
      std::fputs("error: read of stdin failed\n", stderr);
      return EXIT_FAILURE;
    }
    head.assign(bytes.begin(),
                bytes.begin() +
                    static_cast<ptrdiff_t>(std::min<size_t>(bytes.size(), 8)));
    report_or = iamf_tools::Probe(bytes, options);
  } else {
    // `ProbeFile` reads incrementally: descriptors-only probes touch a few
    // KB of even multi-GB files.
    head = ReadHead(input_filename);
    report_or = iamf_tools::ProbeFile(input_filename, options);
  }
  if (!report_or.ok()) {
    std::fprintf(stderr, "error: %s%s\n",
                 std::string(report_or.status().message()).c_str(),
                 SniffHint(head).c_str());
    return EXIT_FAILURE;
  }
  const auto& report = *report_or;

  const std::string format = absl::GetFlag(FLAGS_format);
  std::string out;
  if (format == "json") {
    out = iamf_tools::ProbeReportToJson(report);
  } else if (format == "text") {
    out = FormatText(report);
  } else {
    std::fprintf(stderr, "error: unknown --format=%s (json|text)\n",
                 format.c_str());
    return EXIT_FAILURE;
  }
  std::fputs(out.c_str(), stdout);
  return EXIT_SUCCESS;
}
