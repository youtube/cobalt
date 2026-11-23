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
#include "iamf/cli/probe_json.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/probe.h"

namespace iamf_tools {

namespace {

// Escapes `s` per RFC 8259: backslash, double-quote, and every byte in
// U+0000..U+001F must be escaped; higher bytes (including multi-byte UTF-8
// continuation bytes) pass through unchanged.
std::string EscapeJson(absl::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (const unsigned char c : s) {
    switch (c) {
      case '\\':
        out.append("\\\\");
        break;
      case '"':
        out.append("\\\"");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      default:
        if (c < 0x20) {
          absl::StrAppendFormat(&out, "\\u%04x", c);
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

std::string J(absl::string_view s) {
  return absl::StrCat("\"", EscapeJson(s), "\"");
}

std::string Bool(bool b) { return b ? "true" : "false"; }

template <typename T>
std::string Vec(const std::vector<T>& v) {
  std::vector<std::string> items;
  items.reserve(v.size());
  for (const auto& x : v) items.push_back(absl::StrCat(x));
  return absl::StrCat("[", absl::StrJoin(items, ", "), "]");
}

std::string StrVec(const std::vector<std::string>& v) {
  std::vector<std::string> items;
  items.reserve(v.size());
  for (const auto& x : v) items.push_back(J(x));
  return absl::StrCat("[", absl::StrJoin(items, ", "), "]");
}

std::string JsonCodecConfig(const CodecConfigReport& c) {
  std::string out =
      absl::StrCat("{\"id\": ", c.id, ", \"codec_id\": ", J(c.codec_id),
                   ", \"codec_id_raw\": ", c.codec_id_raw,
                   ", \"num_samples_per_frame\": ", c.num_samples_per_frame,
                   ", \"audio_roll_distance\": ", c.audio_roll_distance,
                   ", \"output_sample_rate\": ", c.output_sample_rate,
                   ", \"input_sample_rate\": ", c.input_sample_rate,
                   ", \"bit_depth\": ", c.bit_depth);
  if (c.opus.has_value()) {
    const auto& o = *c.opus;
    absl::StrAppend(&out, ", \"opus\": {\"version\": ", o.version,
                    ", \"output_channel_count\": ", o.output_channel_count,
                    ", \"pre_skip\": ", o.pre_skip,
                    ", \"input_sample_rate\": ", o.input_sample_rate,
                    ", \"output_gain\": ", o.output_gain,
                    ", \"output_gain_q7_8\": ", o.output_gain_q7_8,
                    ", \"mapping_family\": ", o.mapping_family, "}");
  }
  if (c.lpcm.has_value()) {
    const auto& l = *c.lpcm;
    absl::StrAppend(&out,
                    ", \"lpcm\": {\"sample_format\": ", J(l.sample_format),
                    ", \"sample_format_raw\": ", l.sample_format_raw,
                    ", \"sample_size\": ", l.sample_size,
                    ", \"sample_rate\": ", l.sample_rate, "}");
  }
  if (c.aac.has_value()) {
    const auto& a = *c.aac;
    absl::StrAppend(
        &out,
        ", \"aac\": {\"object_type_indication\": ", a.object_type_indication,
        ", \"stream_type\": ", a.stream_type,
        ", \"upstream\": ", Bool(a.upstream),
        ", \"reserved\": ", Bool(a.reserved),
        ", \"buffer_size_db\": ", a.buffer_size_db,
        ", \"max_bitrate\": ", a.max_bitrate,
        ", \"average_bit_rate\": ", a.average_bit_rate,
        ", \"audio_object_type\": ", a.audio_object_type,
        ", \"sample_frequency_index\": ", J(a.sample_frequency_index),
        ", \"sample_frequency_index_raw\": ", a.sample_frequency_index_raw,
        ", \"channel_configuration\": ", a.channel_configuration, "}");
  }
  if (c.flac.has_value()) {
    std::vector<std::string> blocks;
    blocks.reserve(c.flac->metadata_blocks.size());
    for (const auto& b : c.flac->metadata_blocks) {
      std::string item =
          absl::StrCat("{\"block_type\": ", J(b.block_type),
                       ", \"block_type_raw\": ", b.block_type_raw);
      if (b.is_stream_info) {
        absl::StrAppend(
            &item, ", \"minimum_block_size\": ", b.minimum_block_size,
            ", \"maximum_block_size\": ", b.maximum_block_size,
            ", \"minimum_frame_size\": ", b.minimum_frame_size,
            ", \"maximum_frame_size\": ", b.maximum_frame_size,
            ", \"sample_rate\": ", b.sample_rate,
            ", \"number_of_channels\": ", b.number_of_channels,
            ", \"bits_per_sample\": ", b.bits_per_sample,
            ", \"total_samples_in_stream\": ", b.total_samples_in_stream);
      }
      absl::StrAppend(&item, "}");
      blocks.push_back(std::move(item));
    }
    absl::StrAppend(&out, ", \"flac\": {\"metadata_blocks\": [",
                    absl::StrJoin(blocks, ", "), "]}");
  }
  absl::StrAppend(&out, "}");
  return out;
}

std::string JsonLayer(const ChannelAudioLayerReport& l) {
  std::string item = absl::StrCat(
      "{\"loudspeaker_layout\": ", J(l.loudspeaker_layout),
      ", \"loudspeaker_layout_raw\": ", l.loudspeaker_layout_raw,
      ", \"substream_count\": ", l.substream_count,
      ", \"coupled_substream_count\": ", l.coupled_substream_count,
      ", \"output_gain_is_present_flag\": ",
      Bool(l.output_gain_is_present_flag),
      ", \"recon_gain_is_present_flag\": ", Bool(l.recon_gain_is_present_flag),
      ", \"output_gain_flag\": ", l.output_gain_flag,
      ", \"output_gain\": ", l.output_gain,
      ", \"output_gain_q7_8\": ", l.output_gain_q7_8);
  if (l.expanded_loudspeaker_layout.has_value()) {
    absl::StrAppend(&item, ", \"expanded_loudspeaker_layout\": ",
                    J(*l.expanded_loudspeaker_layout));
  }
  if (l.expanded_loudspeaker_layout_raw.has_value()) {
    absl::StrAppend(&item, ", \"expanded_loudspeaker_layout_raw\": ",
                    *l.expanded_loudspeaker_layout_raw);
  }
  absl::StrAppend(&item, "}");
  return item;
}

std::string JsonParam(const AudioElementParamReport& p) {
  return absl::StrCat(
      "{\"param_type\": ", J(p.param_type),
      ", \"param_type_raw\": ", p.param_type_raw,
      ", \"parameter_id\": ", p.parameter_id,
      ", \"parameter_rate\": ", p.parameter_rate,
      ", \"param_definition_mode\": ", p.param_definition_mode,
      ", \"duration\": ", p.duration,
      ", \"constant_subblock_duration\": ", p.constant_subblock_duration, "}");
}

std::string JsonAudioElement(const AudioElementReport& a) {
  std::string out = absl::StrCat("{\"id\": ", a.id, ", \"type\": ", J(a.type),
                                 ", \"type_raw\": ", a.type_raw,
                                 ", \"reserved\": ", a.reserved,
                                 ", \"codec_config_id\": ", a.codec_config_id,
                                 ", \"num_substreams\": ", a.num_substreams,
                                 ", \"substream_ids\": ", Vec(a.substream_ids));
  std::vector<std::string> params;
  params.reserve(a.params.size());
  for (const auto& p : a.params) params.push_back(JsonParam(p));
  absl::StrAppend(&out, ", \"params\": [", absl::StrJoin(params, ", "), "]");

  if (!a.scalable_layers.empty()) {
    std::vector<std::string> layers;
    layers.reserve(a.scalable_layers.size());
    for (const auto& l : a.scalable_layers) layers.push_back(JsonLayer(l));
    absl::StrAppend(&out, ", \"scalable_layers\": [",
                    absl::StrJoin(layers, ", "), "]");
  }
  if (a.ambisonics.has_value()) {
    const auto& amb = *a.ambisonics;
    absl::StrAppend(&out, ", \"ambisonics\": {\"mode\": ", J(amb.mode),
                    ", \"mode_raw\": ", amb.mode_raw);
    if (amb.order.has_value()) {
      absl::StrAppend(&out, ", \"order\": ", *amb.order);
    }
    if (amb.mono.has_value()) {
      const auto& m = *amb.mono;
      absl::StrAppend(&out, ", \"mono\": {\"output_channel_count\": ",
                      m.output_channel_count,
                      ", \"substream_count\": ", m.substream_count,
                      ", \"channel_mapping\": ", Vec(m.channel_mapping), "}");
    }
    if (amb.projection.has_value()) {
      const auto& p = *amb.projection;
      absl::StrAppend(
          &out, ", \"projection\": {\"output_channel_count\": ",
          p.output_channel_count, ", \"substream_count\": ", p.substream_count,
          ", \"coupled_substream_count\": ", p.coupled_substream_count,
          ", \"demixing_matrix\": ", Vec(p.demixing_matrix), "}");
    }
    absl::StrAppend(&out, "}");
  }
  if (a.channel_layout.has_value()) {
    absl::StrAppend(&out, ", \"channel_layout\": ", J(*a.channel_layout));
  }
  if (a.channel_layout_raw.has_value()) {
    absl::StrAppend(&out, ", \"channel_layout_raw\": ", *a.channel_layout_raw);
  }
  if (a.expanded_channel_layout_raw.has_value()) {
    absl::StrAppend(&out, ", \"expanded_channel_layout_raw\": ",
                    *a.expanded_channel_layout_raw);
  }
  if (a.ambisonics_order.has_value()) {
    absl::StrAppend(&out, ", \"ambisonics_order\": ", *a.ambisonics_order);
  }
  if (a.ambisonics_channels.has_value()) {
    absl::StrAppend(&out,
                    ", \"ambisonics_channels\": ", *a.ambisonics_channels);
  }
  if (a.num_channels.has_value()) {
    absl::StrAppend(&out, ", \"num_channels\": ", *a.num_channels);
  }
  absl::StrAppend(&out, "}");
  return out;
}

std::string JsonParamDef(const ParamDefinitionReport& d) {
  return absl::StrCat(
      "{\"parameter_id\": ", d.parameter_id,
      ", \"parameter_rate\": ", d.parameter_rate,
      ", \"param_definition_mode\": ", d.param_definition_mode,
      ", \"duration\": ", d.duration,
      ", \"constant_subblock_duration\": ", d.constant_subblock_duration, "}");
}

std::string JsonMixGain(const MixGainParamDefinitionReport& g) {
  return absl::StrCat(
      "{\"param_definition\": ", JsonParamDef(g.param_definition),
      ", \"default_mix_gain\": ", g.default_mix_gain,
      ", \"default_mix_gain_q7_8\": ", g.default_mix_gain_q7_8, "}");
}

std::string JsonLoudness(const LoudnessInfoReport& l) {
  std::string out = absl::StrCat(
      "{\"info_type\": ", static_cast<int>(l.info_type),
      ", \"true_peak_present\": ", Bool(l.true_peak_present),
      ", \"anchored_loudness_present\": ", Bool(l.anchored_loudness_present),
      ", \"has_layout_extension\": ", Bool(l.has_layout_extension),
      ", \"integrated_loudness\": ", l.integrated_loudness,
      ", \"digital_peak\": ", l.digital_peak);
  if (l.true_peak.has_value()) {
    absl::StrAppend(&out, ", \"true_peak\": ", *l.true_peak);
  }
  if (!l.anchored_loudness_elements.empty()) {
    std::vector<std::string> els;
    els.reserve(l.anchored_loudness_elements.size());
    for (const auto& e : l.anchored_loudness_elements) {
      els.push_back(absl::StrCat(
          "{\"anchor_element\": ", J(e.anchor_element),
          ", \"anchor_element_raw\": ", e.anchor_element_raw,
          ", \"anchored_loudness\": ", e.anchored_loudness,
          ", \"anchored_loudness_q7_8\": ", e.anchored_loudness_q7_8, "}"));
    }
    absl::StrAppend(&out, ", \"anchored_loudness_elements\": [",
                    absl::StrJoin(els, ", "), "]");
  }
  absl::StrAppend(&out, "}");
  return out;
}

std::string JsonLayout(const MixPresentationLayoutReport& l) {
  std::string out = absl::StrCat(
      "{\"loudness_layout\": {\"layout_type\": ",
      J(l.loudness_layout.layout_type),
      ", \"layout_type_raw\": ", l.loudness_layout.layout_type_raw);
  if (l.loudness_layout.sound_system.has_value()) {
    absl::StrAppend(&out,
                    ", \"sound_system\": ", J(*l.loudness_layout.sound_system));
  }
  if (l.loudness_layout.sound_system_raw.has_value()) {
    absl::StrAppend(
        &out, ", \"sound_system_raw\": ", *l.loudness_layout.sound_system_raw);
  }
  absl::StrAppend(&out, "}, \"loudness\": ", JsonLoudness(l.loudness), "}");
  return out;
}

std::string JsonSubMix(const SubMixReport& s) {
  std::vector<std::string> aes;
  aes.reserve(s.audio_elements.size());
  for (const auto& a : s.audio_elements) {
    aes.push_back(absl::StrCat(
        "{\"audio_element_id\": ", a.audio_element_id,
        ", \"localized_element_annotations\": ",
        StrVec(a.localized_element_annotations),
        ", \"rendering_config\": {\"headphones_rendering_mode\": ",
        J(a.rendering_config.headphones_rendering_mode),
        ", \"headphones_rendering_mode_raw\": ",
        a.rendering_config.headphones_rendering_mode_raw,
        ", \"binaural_filter_profile\": ",
        J(a.rendering_config.binaural_filter_profile),
        ", \"binaural_filter_profile_raw\": ",
        a.rendering_config.binaural_filter_profile_raw, "}",
        ", \"element_mix_gain\": ", JsonMixGain(a.element_mix_gain), "}"));
  }
  std::vector<std::string> layouts;
  layouts.reserve(s.layouts.size());
  for (const auto& l : s.layouts) layouts.push_back(JsonLayout(l));
  return absl::StrCat("{\"audio_elements\": [", absl::StrJoin(aes, ", "), "]",
                      ", \"output_mix_gain\": ", JsonMixGain(s.output_mix_gain),
                      ", \"layouts\": [", absl::StrJoin(layouts, ", "), "]}");
}

std::string JsonMixPresentation(const MixPresentationReport& m) {
  std::vector<std::string> anns;
  anns.reserve(m.annotations.size());
  for (const auto& [lang, label] : m.annotations) {
    anns.push_back(absl::StrCat("{\"language\": ", J(lang),
                                ", \"label\": ", J(label), "}"));
  }
  std::vector<std::string> subs;
  subs.reserve(m.sub_mixes.size());
  for (const auto& s : m.sub_mixes) subs.push_back(JsonSubMix(s));
  std::vector<std::string> tags;
  tags.reserve(m.tags.size());
  for (const auto& t : m.tags) {
    tags.push_back(absl::StrCat("{\"tag_name\": ", J(t.tag_name),
                                ", \"tag_value\": ", J(t.tag_value), "}"));
  }
  std::string out = absl::StrCat(
      "{\"id\": ", m.id, ", \"count_label\": ", m.count_label,
      ", \"num_sub_mixes\": ", m.num_sub_mixes, ", \"annotations\": [",
      absl::StrJoin(anns, ", "), "]", ", \"sub_mixes\": [",
      absl::StrJoin(subs, ", "), "]", ", \"tags\": [",
      absl::StrJoin(tags, ", "), "]", ", \"layouts\": ", StrVec(m.layouts));
  if (m.integrated_loudness_lkfs.has_value()) {
    absl::StrAppend(
        &out, ", \"integrated_loudness_lkfs\": ", *m.integrated_loudness_lkfs);
  }
  if (m.digital_peak_dbfs.has_value()) {
    absl::StrAppend(&out, ", \"digital_peak_dbfs\": ", *m.digital_peak_dbfs);
  }
  if (m.true_peak_dbfs.has_value()) {
    absl::StrAppend(&out, ", \"true_peak_dbfs\": ", *m.true_peak_dbfs);
  }
  absl::StrAppend(&out, "}");
  return out;
}

std::string JsonTemporalUnitScan(const TemporalUnitScanReport& s) {
  std::string out = absl::StrCat(
      "{\"temporal_unit_count\": ", s.temporal_unit_count,
      ", \"audio_frame_count\": ", s.audio_frame_count,
      ", \"parameter_block_count\": ", s.parameter_block_count,
      ", \"temporal_delimiter_count\": ", s.temporal_delimiter_count,
      ", \"audio_frame_parse_errors\": ", s.audio_frame_parse_errors,
      ", \"parameter_block_parse_errors\": ", s.parameter_block_parse_errors,
      ", \"bytes_consumed\": ", s.bytes_consumed,
      ", \"stopped_reason\": ", J(s.stopped_reason));
  if (s.total_samples.has_value()) {
    absl::StrAppend(&out, ", \"total_samples\": ", *s.total_samples);
  }
  if (s.output_sample_rate.has_value()) {
    absl::StrAppend(&out, ", \"output_sample_rate\": ", *s.output_sample_rate);
  }
  if (s.duration_seconds.has_value()) {
    absl::StrAppend(&out, ", \"duration_seconds\": ", *s.duration_seconds);
  }
  std::vector<std::string> frames;
  frames.reserve(s.audio_frames_by_substream.size());
  for (const auto& f : s.audio_frames_by_substream) {
    frames.push_back(absl::StrCat("{\"substream_id\": ", f.substream_id,
                                  ", \"codec_config_id\": ", f.codec_config_id,
                                  ", \"frame_count\": ", f.frame_count,
                                  ", \"total_samples\": ", f.total_samples,
                                  "}"));
  }
  absl::StrAppend(&out, ", \"audio_frames_by_substream\": [",
                  absl::StrJoin(frames, ", "), "]");
  std::vector<std::string> blocks;
  blocks.reserve(s.parameter_blocks.size());
  for (const auto& b : s.parameter_blocks) {
    std::vector<std::string> subs;
    subs.reserve(b.subblocks.size());
    for (const auto& sub : b.subblocks) {
      std::string item =
          absl::StrCat("{\"subblock_duration\": ", sub.subblock_duration);
      if (sub.mix_gain.has_value()) {
        const auto& m = *sub.mix_gain;
        absl::StrAppend(
            &item, ", \"mix_gain\": {\"animation_type\": ", J(m.animation_type),
            ", \"animation_type_raw\": ", m.animation_type_raw,
            ", \"start_point_value\": ", m.start_point_value,
            ", \"start_point_value_q7_8\": ", m.start_point_value_q7_8);
        if (m.end_point_value.has_value()) {
          absl::StrAppend(
              &item, ", \"end_point_value\": ", *m.end_point_value,
              ", \"end_point_value_q7_8\": ", *m.end_point_value_q7_8);
        }
        if (m.control_point_value.has_value()) {
          absl::StrAppend(
              &item, ", \"control_point_value\": ", *m.control_point_value,
              ", \"control_point_value_q7_8\": ", *m.control_point_value_q7_8);
        }
        if (m.control_point_relative_time.has_value()) {
          absl::StrAppend(&item, ", \"control_point_relative_time\": ",
                          *m.control_point_relative_time);
        }
        absl::StrAppend(&item, "}");
      }
      if (sub.demixing.has_value()) {
        absl::StrAppend(&item, ", \"demixing\": {\"dmixp_mode\": ",
                        J(sub.demixing->dmixp_mode),
                        ", \"dmixp_mode_raw\": ", sub.demixing->dmixp_mode_raw,
                        ", \"reserved\": ", sub.demixing->reserved, "}");
      }
      if (sub.recon_gain.has_value()) {
        std::vector<std::string> layers;
        layers.reserve(sub.recon_gain->layers.size());
        for (const auto& l : sub.recon_gain->layers) {
          layers.push_back(absl::StrCat(
              "{\"layer_index\": ", l.layer_index, ", \"recon_gain_flag\": ",
              l.recon_gain_flag, ", \"recon_gain\": ", Vec(l.recon_gain), "}"));
        }
        absl::StrAppend(&item, ", \"recon_gain\": {\"layers\": [",
                        absl::StrJoin(layers, ", "), "]}");
      }
      absl::StrAppend(&item, "}");
      subs.push_back(std::move(item));
    }
    blocks.push_back(absl::StrCat(
        "{\"parameter_id\": ", b.parameter_id, ", \"param_type\": ",
        J(b.param_type), ", \"param_type_raw\": ", b.param_type_raw,
        ", \"start_timestamp\": ", b.start_timestamp, ", \"end_timestamp\": ",
        b.end_timestamp, ", \"duration\": ", b.duration,
        ", \"constant_subblock_duration\": ", b.constant_subblock_duration,
        ", \"subblocks\": [", absl::StrJoin(subs, ", "), "]}"));
  }
  absl::StrAppend(&out, ", \"parameter_blocks\": [",
                  absl::StrJoin(blocks, ", "), "]}");
  return out;
}

}  // namespace

std::string ProbeReportToJson(const ProbeReport& r) {
  std::vector<std::string> ccs;
  ccs.reserve(r.codec_configs.size());
  for (const auto& c : r.codec_configs) {
    ccs.push_back(absl::StrCat("    ", JsonCodecConfig(c)));
  }
  std::vector<std::string> aes;
  aes.reserve(r.audio_elements.size());
  for (const auto& a : r.audio_elements) {
    aes.push_back(absl::StrCat("    ", JsonAudioElement(a)));
  }
  std::vector<std::string> mps;
  mps.reserve(r.mix_presentations.size());
  for (const auto& m : r.mix_presentations) {
    mps.push_back(absl::StrCat("    ", JsonMixPresentation(m)));
  }

  std::string out = "{\n";
  absl::StrAppend(&out, "  \"primary_profile\": ", J(r.primary_profile), ",\n");
  absl::StrAppend(&out, "  \"primary_profile_raw\": ", r.primary_profile_raw,
                  ",\n");
  absl::StrAppend(&out, "  \"additional_profile\": ", J(r.additional_profile),
                  ",\n");
  absl::StrAppend(
      &out, "  \"additional_profile_raw\": ", r.additional_profile_raw, ",\n");
  absl::StrAppend(&out, "  \"descriptor_bytes_consumed\": ",
                  r.descriptor_bytes_consumed, ",\n");
  if (r.descriptor_total_samples.has_value()) {
    absl::StrAppend(&out, "  \"descriptor_total_samples\": ",
                    *r.descriptor_total_samples, ",\n");
  }
  if (r.descriptor_duration_seconds.has_value()) {
    absl::StrAppend(&out, "  \"descriptor_duration_seconds\": ",
                    *r.descriptor_duration_seconds, ",\n");
  }
  absl::StrAppend(&out, "  \"codec_configs\": [\n", absl::StrJoin(ccs, ",\n"),
                  "\n  ],\n");
  absl::StrAppend(&out, "  \"audio_elements\": [\n", absl::StrJoin(aes, ",\n"),
                  "\n  ],\n");
  absl::StrAppend(&out, "  \"mix_presentations\": [\n",
                  absl::StrJoin(mps, ",\n"), "\n  ]");
  if (r.temporal_unit_scan.has_value()) {
    absl::StrAppend(&out, ",\n  \"temporal_unit_scan\": ",
                    JsonTemporalUnitScan(*r.temporal_unit_scan));
  }
  absl::StrAppend(&out, "\n}\n");
  return out;
}

}  // namespace iamf_tools
