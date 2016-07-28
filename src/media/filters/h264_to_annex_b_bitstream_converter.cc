// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h264_to_annex_b_bitstream_converter.h"

#include "base/logging.h"

namespace media {

static const uint8 kStartCodePrefix[3] = {0, 0, 1};

// Helper function which determines whether NAL unit of given type marks
// access unit boundary.
static bool IsAccessUnitBoundaryNal(int nal_unit_type) {
  // Check if this packet marks access unit boundary by checking the
  // packet type.
  if (nal_unit_type == 6 ||  // Supplemental enhancement information
      nal_unit_type == 7 ||  // Picture parameter set
      nal_unit_type == 8 ||  // Sequence parameter set
      nal_unit_type == 9 ||  // Access unit delimiter
      (nal_unit_type >= 14 && nal_unit_type <= 18)) {  // Reserved types
    return true;
  }
  return false;
}

H264ToAnnexBBitstreamConverter::H264ToAnnexBBitstreamConverter()
    : configuration_processed_(false),
      first_nal_unit_in_access_unit_(true),
      nal_unit_length_field_width_(0) {
}

H264ToAnnexBBitstreamConverter::~H264ToAnnexBBitstreamConverter() {}

uint32 H264ToAnnexBBitstreamConverter::ParseConfigurationAndCalculateSize(
    const uint8* configuration_record,
    uint32 configuration_record_size) {
  // FFmpeg's AVCodecContext's extradata field contains the Decoder Specific
  // Information from MP4 headers that contain the H.264 SPS and PPS members.
  // ISO 14496-15 Chapter 5.2.4 AVCDecoderConfigurationRecord.
  // AVCConfigurationRecord must be at least 7 bytes long.
  if (configuration_record == NULL || configuration_record_size < 7) {
    return 0;  // Error: invalid input
  }
  const uint8* decoder_configuration = configuration_record;
  uint32 parameter_set_size_bytes = 0;

  // We can skip the four first bytes as they're only profile information
  decoder_configuration += 4;
  // Fifth byte's two LSBs contain the interleaving field's size minus one
  uint8 size_of_len_field = (*decoder_configuration & 0x3) + 1;
  if (size_of_len_field != 1 && size_of_len_field != 2 &&
      size_of_len_field != 4) {
    return 0;  // Error: invalid input, NAL unit field len is not correct
  }
  decoder_configuration++;
  // Sixth byte's five LSBs contain the number of SPSs
  uint8 sps_count = *decoder_configuration & 0x1F;
  decoder_configuration++;
  // Then we have N * SPS's with two byte length field and actual SPS
  while (sps_count-- > 0) {
    if ((decoder_configuration - configuration_record) + 2 >
         static_cast<int32>(configuration_record_size)) {
      return 0;  // Error: ran out of data
    }
    uint16 sps_len = decoder_configuration[0] << 8 | decoder_configuration[1];
    decoder_configuration += 2;
    // write the SPS to output, always with zero byte + start code prefix
    parameter_set_size_bytes += 1 + sizeof(kStartCodePrefix);
    decoder_configuration += sps_len;
    parameter_set_size_bytes += sps_len;
  }
  // Then we have the numner of pps in one byte
  uint8 pps_count = *decoder_configuration;
  decoder_configuration++;
  // And finally, we have N * PPS with two byte length field and actual PPS
  while (pps_count-- > 0) {
    if ((decoder_configuration - configuration_record) + 2 >
         static_cast<int32>(configuration_record_size)) {
      return 0;  // Error: ran out of data
    }
    uint16 pps_len = decoder_configuration[0] << 8 | decoder_configuration[1];
    decoder_configuration += 2;
    // write the SPS to output, always with zero byte + start code prefix
    parameter_set_size_bytes += 1 + sizeof(kStartCodePrefix);
    decoder_configuration += pps_len;
    parameter_set_size_bytes += pps_len;
  }
  // We're done processing the AVCDecoderConfigurationRecord,
  // store the needed information for parsing actual payload
  nal_unit_length_field_width_ = size_of_len_field;
  configuration_processed_ = true;
  return parameter_set_size_bytes;
}

uint32 H264ToAnnexBBitstreamConverter::CalculateNeededOutputBufferSize(
    const uint8* input,
    uint32 input_size) const {
  uint32 output_size = 0;
  uint32 data_left = input_size;
  bool first_nal_in_this_access_unit = first_nal_unit_in_access_unit_;

  if (input == NULL || input_size == 0) {
    return 0;  // Error: invalid input data
  }
  if (!configuration_processed_) {
    return 0;  // Error: configuration not handled, we don't know nal unit width
  }
  CHECK(nal_unit_length_field_width_ == 1 ||
        nal_unit_length_field_width_ == 2 ||
        nal_unit_length_field_width_ == 4);

  // Then add the needed size for the actual packet
  while (data_left > 0) {
    // Read the next NAL unit length from the input buffer
    uint8 size_of_len_field;
    uint32 nal_unit_length;
    for (nal_unit_length = 0, size_of_len_field = nal_unit_length_field_width_;
         size_of_len_field > 0;
         input++, size_of_len_field--, data_left--) {
      nal_unit_length <<= 8;
      nal_unit_length |= *input;
    }

    if (nal_unit_length == 0) {
      break;  // Signifies that no more data left in the buffer
    } else if (nal_unit_length > data_left) {
      return 0;  // Error: Not enough data for correct conversion
    }
    data_left -= nal_unit_length;

    // five least significant bits of first NAL unit byte signify nal_unit_type
    int nal_unit_type = *input & 0x1F;
    if (first_nal_in_this_access_unit ||
        IsAccessUnitBoundaryNal(nal_unit_type)) {
      output_size += 1;  // Extra zero_byte for these nal units
      first_nal_in_this_access_unit = false;
    }
    // Start code prefix
    output_size += sizeof(kStartCodePrefix);
    // Actual NAL unit size
    output_size += nal_unit_length;
    input += nal_unit_length;
    // No need for trailing zero bits
  }
  return output_size;
}

bool H264ToAnnexBBitstreamConverter::ConvertAVCDecoderConfigToByteStream(
    const uint8* input,
    uint32 input_size,
    uint8* output,
    uint32* output_size) {
  uint8* outscan = output;
  // FFmpeg's AVCodecContext's extradata field contains the Decoder Specific
  // Information from MP4 headers that contain the H.264 SPS and PPS members.
  // ISO 14496-15 Chapter 5.2.4 AVCDecoderConfigurationRecord.
  const uint8* decoder_configuration = input;
  uint32 decoderconfiguration_size = input_size;
  uint32 out_size = 0;

  if (decoder_configuration == NULL || decoderconfiguration_size == 0) {
    return 0;  // Error: input invalid
  }

  // We can skip the four first bytes as they're only profile information.
  decoder_configuration += 4;
  // Fifth byte's two LSBs contain the interleaving field's size minus one
  uint8 size_of_len_field = (*decoder_configuration & 0x3) + 1;
  if (size_of_len_field != 1 && size_of_len_field != 2 &&
      size_of_len_field != 4) {
    return 0;  // Error: invalid input, NAL unit field len is not correct
  }
  decoder_configuration++;
  // Sixth byte's five LSBs contain the number of SPSs
  uint8 sps_count = *decoder_configuration & 0x1F;
  decoder_configuration++;
  // Then we have N * SPS's with two byte length field and actual SPS
  while (sps_count-- > 0) {
    uint16 sps_len = decoder_configuration[0] << 8 |
                     decoder_configuration[1];
    decoder_configuration += 2;
    if (out_size + 1 + sizeof(kStartCodePrefix) + sps_len >
        *output_size) {
      *output_size = 0;
      return 0;  // too small output buffer;
    }
    // write the SPS to output, always with zero byte + start code prefix
    *outscan = 0;  // zero byte
    outscan += 1;
    memcpy(outscan, kStartCodePrefix, sizeof(kStartCodePrefix));
    outscan += sizeof(kStartCodePrefix);
    memcpy(outscan, decoder_configuration, sps_len);
    decoder_configuration += sps_len;
    outscan += sps_len;
    out_size += 1 + sizeof(kStartCodePrefix) + sps_len;
  }
  // Then we have the numner of pps in one byte
  uint8 pps_count = *decoder_configuration;
  decoder_configuration++;
  // And finally, we have N * PPS with two byte length field and actual PPS
  while (pps_count-- > 0) {
    uint16 pps_len = decoder_configuration[0] << 8 | decoder_configuration[1];
    decoder_configuration += 2;
    if (out_size + 1 + sizeof(kStartCodePrefix) + pps_len >
        *output_size) {
      *output_size = 0;
      return 0;  // too small output buffer;
    }
    // write the SPS to output, always with zero byte + start code prefix
    *outscan = 0;  // zero byte
    outscan += 1;
    memcpy(outscan, kStartCodePrefix, sizeof(kStartCodePrefix));
    outscan += sizeof(kStartCodePrefix);
    memcpy(outscan, decoder_configuration, pps_len);
    decoder_configuration += pps_len;
    outscan += pps_len;
    out_size += 1 + sizeof(kStartCodePrefix) + pps_len;
  }
  // We're done processing the AVCDecoderConfigurationRecord, store the needed
  // information
  nal_unit_length_field_width_ = size_of_len_field;
  configuration_processed_ = true;
  *output_size = out_size;
  return true;
}

bool H264ToAnnexBBitstreamConverter::ConvertNalUnitStreamToByteStream(
    const uint8* input, uint32 input_size,
    uint8* output, uint32* output_size) {
  const uint8* inscan = input;  // We read the input from here progressively
  uint8* outscan = output;  // We write the output to here progressively
  uint32 data_left = input_size;

  if (inscan == NULL || input_size == 0 ||
      outscan == NULL || *output_size == 0) {
    *output_size = 0;
    return false;  // Error: invalid input
  }

  // NAL unit width should be known at this point
  CHECK(nal_unit_length_field_width_ == 1 ||
        nal_unit_length_field_width_ == 2 ||
        nal_unit_length_field_width_ == 4);

  // Do the actual conversion for the actual input packet
  while (data_left > 0) {
    uint8 i;
    uint32 nal_unit_length;

    // Read the next NAL unit length from the input buffer by scanning
    // the input stream with the specific length field width
    for (nal_unit_length = 0, i = nal_unit_length_field_width_;
         i > 0 && data_left > 0;
         inscan++, i--, data_left--) {
      nal_unit_length <<= 8;
      nal_unit_length |= *inscan;
    }

    if (nal_unit_length == 0) {
      break;  // Successful conversion, end of buffer
    } else if (nal_unit_length > data_left) {
      *output_size = 0;
      return false;  // Error: not enough data for correct conversion
    }

    uint32 start_code_len;
    first_nal_unit_in_access_unit_ ?
        start_code_len = sizeof(kStartCodePrefix) + 1 :
        start_code_len = sizeof(kStartCodePrefix);
    if (static_cast<uint32>(outscan - output) +
        start_code_len + nal_unit_length > *output_size) {
      *output_size = 0;
      return false;  // Error: too small output buffer
    }

    // Five least significant bits of first NAL unit byte signify
    // nal_unit_type.
    int nal_unit_type = *inscan & 0x1F;

    // Check if this packet marks access unit boundary by checking the
    // packet type.
    if (IsAccessUnitBoundaryNal(nal_unit_type)) {
      first_nal_unit_in_access_unit_ = true;
    }

    // Write extra zero-byte before start code prefix if this packet
    // signals next access unit.
    if (first_nal_unit_in_access_unit_) {
      *outscan = 0;
      outscan++;
      first_nal_unit_in_access_unit_ = false;
    }

    // No need to write leading zero bits.
    // Write start-code prefix.
    memcpy(outscan, kStartCodePrefix, sizeof(kStartCodePrefix));
    outscan += sizeof(kStartCodePrefix);
    // Then write the actual NAL unit from the input buffer.
    memcpy(outscan, inscan, nal_unit_length);
    inscan += nal_unit_length;
    data_left -= nal_unit_length;
    outscan += nal_unit_length;
    // No need for trailing zero bits.
  }
  // Successful conversion, output the freshly allocated bitstream buffer.
  *output_size = static_cast<uint32>(outscan - output);
  return true;
}

}  // namespace media
