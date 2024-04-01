// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"

#include "base/big_endian.h"
#include "base/containers/flat_set.h"

namespace media {

H264AnnexBToAvcBitstreamConverter::H264AnnexBToAvcBitstreamConverter() {
  // These parts of configuration never change.
  config_.version = 1;
  config_.length_size = 4;
}

H264AnnexBToAvcBitstreamConverter::~H264AnnexBToAvcBitstreamConverter() =
    default;

const mp4::AVCDecoderConfigurationRecord&
H264AnnexBToAvcBitstreamConverter::GetCurrentConfig() {
  return config_;
}

Status H264AnnexBToAvcBitstreamConverter::ConvertChunk(
    const base::span<const uint8_t> input,
    base::span<uint8_t> output,
    bool* config_changed_out,
    size_t* size_out) {
  std::vector<H264NALU> slice_units;
  size_t data_size = 0;
  bool config_changed = false;
  H264NALU nalu;
  H264Parser::Result result;
  int new_active_sps_id = -1;
  int new_active_pps_id = -1;
  // Sets of SPS and PPS ids to be included into the decoder config.
  // They contain
  // - all SPS and PPS units encountered in the input chunk;
  // - any SPS referenced by PPS units encountered in the input;
  // - The active SPS/PPS pair.
  base::flat_set<int> sps_to_include;
  base::flat_set<int> pps_to_include;

  // Scan input buffer looking for two main types of NALUs
  //  1. SPS and PPS. They'll be added to the AVC configuration |config_|
  //     and will *not* be copied to |output|.
  //  2. Slices. They'll being copied into the output buffer, but also affect
  //     what configuration (profile and level) is active now.
  parser_.SetStream(input.data(), input.size());
  while ((result = parser_.AdvanceToNextNALU(&nalu)) != H264Parser::kEOStream) {
    if (result == H264Parser::kUnsupportedStream)
      return Status(StatusCode::kH264ParsingError, "Unsupported H.264 stream");

    if (result != H264Parser::kOk)
      return Status(StatusCode::kH264ParsingError,
                    "Failed to parse H.264 stream");

    switch (nalu.nal_unit_type) {
      case H264NALU::kAUD: {
        break;
      }
      case H264NALU::kSPS: {
        int sps_id = -1;
        result = parser_.ParseSPS(&sps_id);
        if (result == H264Parser::kUnsupportedStream)
          return Status(StatusCode::kH264ParsingError, "Unsupported SPS");

        if (result != H264Parser::kOk)
          return Status(StatusCode::kH264ParsingError, "Could not parse SPS");

        id2sps_.insert_or_assign(sps_id,
                                 blob(nalu.data, nalu.data + nalu.size));
        sps_to_include.insert(sps_id);
        config_changed = true;
        break;
      }

      case H264NALU::kSPSExt: {
        // SPS extensions are not supported yet.
        break;
      }

      case H264NALU::kPPS: {
        int pps_id = -1;
        result = parser_.ParsePPS(&pps_id);
        if (result == H264Parser::kUnsupportedStream)
          return Status(StatusCode::kH264ParsingError, "Unsupported PPS");

        if (result != H264Parser::kOk)
          return Status(StatusCode::kH264ParsingError, "Could not parse PPS");

        id2pps_.insert_or_assign(pps_id,
                                 blob(nalu.data, nalu.data + nalu.size));
        pps_to_include.insert(pps_id);
        if (auto* pps = parser_.GetPPS(pps_id))
          sps_to_include.insert(pps->seq_parameter_set_id);
        config_changed = true;
        break;
      }

      case H264NALU::kSliceDataA:
      case H264NALU::kSliceDataB:
      case H264NALU::kSliceDataC:
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice: {
        H264SliceHeader slice_hdr;
        result = parser_.ParseSliceHeader(nalu, &slice_hdr);
        if (result != H264Parser::kOk) {
          return Status(StatusCode::kH264ParsingError,
                        "Could not parse slice header");
        }

        const H264PPS* pps = parser_.GetPPS(slice_hdr.pic_parameter_set_id);
        if (!pps) {
          return Status(StatusCode::kH264ParsingError,
                        "PPS requested by slice not found");
        }

        const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
        if (!sps) {
          return Status(StatusCode::kH264ParsingError,
                        "SPS requested by PPS not found");
        }
        new_active_pps_id = pps->pic_parameter_set_id;
        new_active_sps_id = sps->seq_parameter_set_id;
        if (new_active_sps_id != active_sps_id_ ||
            new_active_pps_id != active_pps_id_) {
          pps_to_include.insert(new_active_pps_id);
          sps_to_include.insert(new_active_sps_id);
          config_changed = true;
        }
      }
        FALLTHROUGH;
      default:
        slice_units.push_back(nalu);
        data_size += config_.length_size + nalu.size;
        break;
    }
  }

  if (size_out)
    *size_out = data_size;
  if (data_size > output.size()) {
    return Status(StatusCode::kH264BufferTooSmall,
                  "Not enough space in the output buffer.");
  }

  // Write slice NALUs from the input buffer to the output buffer
  // prefixing them with size.
  base::BigEndianWriter writer(reinterpret_cast<char*>(output.data()),
                               output.size());
  for (auto& unit : slice_units) {
    bool written_ok =
        writer.WriteU32(unit.size) && writer.WriteBytes(unit.data, unit.size);
    if (!written_ok) {
      return Status(StatusCode::kH264BufferTooSmall,
                    "Not enough space in the output buffer.");
    }
  }

  DCHECK_LE(writer.remaining(), output.size());
  size_t bytes_written = output.size() - writer.remaining();
  DCHECK_EQ(bytes_written, data_size);

  // Now when we are sure that everything is written and fits nicely,
  // we can update parts of the |config_| that were changed by this data chunk.
  if (config_changed) {
    if (new_active_sps_id < 0)
      new_active_sps_id = active_sps_id_;
    if (new_active_pps_id < 0)
      new_active_pps_id = active_pps_id_;

    const H264SPS* active_sps = parser_.GetSPS(new_active_sps_id);
    if (!active_sps) {
      return Status(StatusCode::kH264ParsingError,
                    "No slices referring to SPS. No way to know configuration");
    }

    active_pps_id_ = new_active_pps_id;
    active_sps_id_ = new_active_sps_id;

    config_.sps_list.clear();
    config_.pps_list.clear();

    // flat_set is iterated in key-order
    for (int id : sps_to_include)
      config_.sps_list.push_back(id2sps_[id]);

    for (int id : pps_to_include)
      config_.pps_list.push_back(id2pps_[id]);

    config_.profile_indication = active_sps->profile_idc;
    config_.profile_compatibility =
        (active_sps->constraint_set0_flag ? 1 : 0) |
        (active_sps->constraint_set1_flag ? (1 << 1) : 0) |
        (active_sps->constraint_set2_flag ? (1 << 2) : 0) |
        (active_sps->constraint_set3_flag ? (1 << 3) : 0);
    config_.avc_level = active_sps->level_idc;
  }

  if (config_changed_out)
    *config_changed_out = config_changed;

  return Status();
}

}  // namespace media