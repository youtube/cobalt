// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// H264ToAnnexBBitstreamConverter is a class to convert H.264 bitstream from
// MP4 format (as specified in ISO/IEC 14496-15) into H.264 bytestream
// (as specified in ISO/IEC 14496-10 Annex B).
class MEDIA_EXPORT H264ToAnnexBBitstreamConverter {
 public:
  H264ToAnnexBBitstreamConverter();
  ~H264ToAnnexBBitstreamConverter();

  // Parses the global AVCDecoderConfigurationRecord from the file format's
  // headers. Converter will remember the field length from the configuration
  // headers after this.
  //
  // Parameters
  //   configuration_record
  //     Pointer to buffer containing AVCDecoderConfigurationRecord.
  //   configuration_record_size
  //     Size of the buffer in bytes.
  //
  // Returns
  //   Required buffer size for AVCDecoderConfigurationRecord when converted
  //   to bytestream format, or 0 if could not determine the configuration
  //   from the input buffer.
  uint32 ParseConfigurationAndCalculateSize(const uint8* configuration_record,
                                            uint32 configuration_record_size);

  // Calculates needed buffer size for the bitstream converted into bytestream.
  // Lightweight implementation that does not do the actual conversion.
  //
  // Parameters
  //   configuration_record
  //     Pointer to buffer containing AVCDecoderConfigurationRecord.
  //   configuration_record_size
  //     Size of the buffer in bytes.
  //
  // Returns
  //   Required buffer size for the input NAL unit buffer when converted
  //   to bytestream format, or 0 if could not determine the configuration
  //   from the input buffer.
  uint32 CalculateNeededOutputBufferSize(const uint8* input,
                                         uint32 input_size) const;

  // ConvertAVCDecoderConfigToByteStream converts the
  // AVCDecoderConfigurationRecord from the MP4 headers to bytestream format.
  // Client is responsible for making sure the output buffer is large enough
  // to hold the output data. Client can precalculate the needed output buffer
  // size by using ParseConfigurationAndCalculateSize.
  //
  // In case of failed conversion object H264BitstreamConverter may have written
  // some bytes to buffer pointed by pinput but user should ignore those bytes.
  // None of the outputs should be considered valid.
  //
  // Parameters
  //   pinput
  //     Pointer to buffer containing AVCDecoderConfigurationRecord.
  //   input_size
  //     Size of the buffer in bytes.
  //   poutput
  //     Pointer to buffer where the output should be written to.
  //   poutput_size (i/o)
  //     Pointer to the size of the output buffer. Will contain the number of
  //     bytes written to output after successful call.
  //
  // Returns
  //    true  if successful conversion
  //    false if conversion not successful (poutput_size will hold the amount
  //          of converted data)
  bool ConvertAVCDecoderConfigToByteStream(const uint8* input,
                                           uint32 input_size,
                                           uint8* output,
                                           uint32* output_size);

  // ConvertNalUnitStreamToByteStream converts the NAL unit from MP4 format
  // to bytestream format. Client is responsible for making sure the output
  // buffer is large enough to hold the output data. Client can precalculate the
  // needed output buffer size by using CalculateNeededOutputBufferSize.
  //
  // In case of failed conversion object H264BitstreamConverter may have written
  // some bytes to buffer pointed by pinput but user should ignore those bytes.
  // None of the outputs should be considered valid.
  //
  // Parameters
  //   pinput
  //     Pointer to buffer containing AVCDecoderConfigurationRecord.
  //   input_size
  //     Size of the buffer in bytes.
  //   poutput
  //     Pointer to buffer where the output should be written to.
  //   poutput_size (i/o)
  //     Pointer to the size of the output buffer. Will contain the number of
  //     bytes written to output after successful call.
  //
  // Returns
  //    true  if successful conversion
  //    false if conversion not successful (poutput_size will hold the amount
  //          of converted data)
  bool ConvertNalUnitStreamToByteStream(const uint8* input, uint32 input_size,
                                        uint8* output, uint32* output_size);

 private:
  // Flag for indicating whether global parameter sets have been processed.
  bool configuration_processed_;
  // Flag for indicating whether next NAL unit starts new access unit.
  bool first_nal_unit_in_access_unit_;
  // Variable to hold interleaving field's length in bytes.
  uint8 nal_unit_length_field_width_;

  DISALLOW_COPY_AND_ASSIGN(H264ToAnnexBBitstreamConverter);
};

}  // namespace media

#endif  // MEDIA_FILTERS_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

