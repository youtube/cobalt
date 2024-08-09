// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <va/va.h>

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/chromium/media/gpu/vaapi/fuzzers/jpeg_decoder/jpeg_decoder_fuzzer_input.pb.h"
#include "third_party/chromium/media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "third_party/chromium/media/gpu/vaapi/vaapi_utils.h"
#include "third_party/chromium/media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/chromium/media/parsers/jpeg_parser.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/gfx/geometry/size.h"

// This file defines a fuzzer test for the JPEG decoding functionality of the
// VA-API. To do the fuzzing, we re-use code from media_m96::VaapiJpegDecoder: we
// poke at its internals to get past the parsing and image support validation.
// That way, we can inject a large variety of data into the VA-API driver and
// exercise more paths than if we just pass raw data to the
// media_m96::VaapiJpegDecoder using its public API.
//
// The VA-API does not take raw JPEG data. Instead, it expects that some
// pre-parsing was done. Therefore, we feed the data as a
// media_m96::JpegParseResult. To generate the fuzzing inputs, we use
// libprotobuf-mutator, so we define a protobuf that mirrors the structure of
// media_m96::JpegParseResult.
//
// This fuzzer should be run on a real device that supports the VA-API.
// Additionally, libva and the backend user-space VA-API driver (plus its
// dependencies) should be instrumented for fuzzing.

namespace {

// Converts |proto_huffman_table| to a media_m96::JpegHuffmanTable which can be used
// to populate the Huffman table fields of a media_m96::JpegParseResult.
media_m96::JpegHuffmanTable ConvertToJpegHuffmanTable(
    const media_m96::fuzzing::JpegHuffmanTable& proto_huffman_table) {
  media_m96::JpegHuffmanTable huffman_table{};
  huffman_table.valid = proto_huffman_table.valid();
  memcpy(huffman_table.code_length, proto_huffman_table.code_length().data(),
         std::min(std::size(huffman_table.code_length),
                  proto_huffman_table.code_length().size()));
  memcpy(huffman_table.code_value, proto_huffman_table.code_value().data(),
         std::min(std::size(huffman_table.code_value),
                  proto_huffman_table.code_value().size()));
  return huffman_table;
}

// Converts |proto_parse_result| to a media_m96::JpegParseResult that can be used as
// the input for VaapiJpegDecoder::SubmitBuffers(). The data field of the return
// value will point to the same memory as |proto_parse_result|.data().data(), so
// we assume |proto_parse_result| will live for as long as that data needs to be
// used.
media_m96::JpegParseResult ConvertToJpegParseResult(
    const media_m96::fuzzing::JpegParseResult& proto_parse_result) {
  media_m96::JpegParseResult parse_result{};

  // Convert the frame header.
  parse_result.frame_header.visible_width =
      proto_parse_result.frame_header().visible_width() & 0xFFFF;
  parse_result.frame_header.visible_height =
      proto_parse_result.frame_header().visible_height() & 0xFFFF;
  parse_result.frame_header.coded_width =
      proto_parse_result.frame_header().coded_width() & 0xFFFF;
  parse_result.frame_header.coded_height =
      proto_parse_result.frame_header().coded_height() & 0xFFFF;

  const size_t frame_header_num_components =
      std::min(std::size(parse_result.frame_header.components),
               base::checked_cast<size_t>(
                   proto_parse_result.frame_header().components_size()));
  for (size_t i = 0; i < frame_header_num_components; i++) {
    const auto& input_jpeg_component =
        proto_parse_result.frame_header().components()[i];
    parse_result.frame_header.components[i].id =
        input_jpeg_component.id() & 0xFF;
    parse_result.frame_header.components[i].horizontal_sampling_factor =
        input_jpeg_component.horizontal_sampling_factor() & 0xFF;
    parse_result.frame_header.components[i].vertical_sampling_factor =
        input_jpeg_component.vertical_sampling_factor() & 0xFF;
    parse_result.frame_header.components[i].quantization_table_selector =
        input_jpeg_component.quantization_table_selector() & 0xFF;
  }
  parse_result.frame_header.num_components =
      static_cast<uint8_t>(frame_header_num_components);

  // Convert the DC/AC Huffman tables.
  const size_t num_dc_tables =
      std::min(std::size(parse_result.dc_table),
               base::checked_cast<size_t>(proto_parse_result.dc_table_size()));
  for (size_t i = 0; i < num_dc_tables; i++) {
    parse_result.dc_table[i] =
        ConvertToJpegHuffmanTable(proto_parse_result.dc_table()[i]);
  }
  const size_t num_ac_tables =
      std::min(std::size(parse_result.ac_table),
               base::checked_cast<size_t>(proto_parse_result.ac_table_size()));
  for (size_t i = 0; i < num_ac_tables; i++) {
    parse_result.ac_table[i] =
        ConvertToJpegHuffmanTable(proto_parse_result.ac_table()[i]);
  }

  // Convert the quantization tables.
  const size_t num_q_tables =
      std::min(std::size(parse_result.q_table),
               base::checked_cast<size_t>(proto_parse_result.q_table_size()));
  for (size_t i = 0; i < num_q_tables; i++) {
    const media_m96::fuzzing::JpegQuantizationTable& input_q_table =
        proto_parse_result.q_table()[i];
    parse_result.q_table[i].valid = input_q_table.valid();
    memcpy(parse_result.q_table[i].value, input_q_table.value().data(),
           std::min(std::size(parse_result.q_table[i].value),
                    input_q_table.value().size()));
  }

  // Convert the scan header.
  const size_t scan_num_components = std::min(
      std::size(parse_result.scan.components),
      base::checked_cast<size_t>(proto_parse_result.scan().components_size()));
  for (size_t i = 0; i < scan_num_components; i++) {
    const media_m96::fuzzing::JpegScanHeader::Component& input_component =
        proto_parse_result.scan().components()[i];
    parse_result.scan.components[i].component_selector =
        input_component.component_selector() & 0xFF;
    parse_result.scan.components[i].dc_selector =
        input_component.dc_selector() & 0xFF;
    parse_result.scan.components[i].ac_selector =
        input_component.ac_selector() & 0xFF;
  }
  parse_result.scan.num_components = static_cast<uint8_t>(scan_num_components);

  // Convert the coded data. Note that we don't do a deep copy, so we assume
  // that |proto_parse_result| will live for as long as |parse_result|.data is
  // needed.
  parse_result.data = proto_parse_result.data().data();
  parse_result.data_size = proto_parse_result.data().size();

  // Convert the rest of the fields.
  parse_result.restart_interval =
      proto_parse_result.restart_interval() & 0xFFFF;
  parse_result.image_size =
      base::strict_cast<size_t>(proto_parse_result.image_size());

  return parse_result;
}

unsigned int ConvertToVARTFormat(
    media_m96::fuzzing::VARTFormat proto_picture_va_rt_format) {
  switch (proto_picture_va_rt_format) {
    case media_m96::fuzzing::VARTFormat::INVALID:
      return media_m96::kInvalidVaRtFormat;
    case media_m96::fuzzing::VARTFormat::YUV420:
      return VA_RT_FORMAT_YUV420;
    case media_m96::fuzzing::VARTFormat::YUV422:
      return VA_RT_FORMAT_YUV422;
    case media_m96::fuzzing::VARTFormat::YUV444:
      return VA_RT_FORMAT_YUV444;
    case media_m96::fuzzing::VARTFormat::YUV411:
      return VA_RT_FORMAT_YUV411;
    case media_m96::fuzzing::VARTFormat::YUV400:
      return VA_RT_FORMAT_YUV400;
    case media_m96::fuzzing::VARTFormat::YUV420_10:
      return VA_RT_FORMAT_YUV420_10;
    case media_m96::fuzzing::VARTFormat::YUV422_10:
      return VA_RT_FORMAT_YUV422_10;
    case media_m96::fuzzing::VARTFormat::YUV444_10:
      return VA_RT_FORMAT_YUV444_10;
    case media_m96::fuzzing::VARTFormat::YUV420_12:
      return VA_RT_FORMAT_YUV420_12;
    case media_m96::fuzzing::VARTFormat::YUV422_12:
      return VA_RT_FORMAT_YUV422_12;
    case media_m96::fuzzing::VARTFormat::YUV444_12:
      return VA_RT_FORMAT_YUV444_12;
    case media_m96::fuzzing::VARTFormat::RGB16:
      return VA_RT_FORMAT_RGB16;
    case media_m96::fuzzing::VARTFormat::RGB32:
      return VA_RT_FORMAT_RGB32;
    case media_m96::fuzzing::VARTFormat::RGBP:
      return VA_RT_FORMAT_RGBP;
    case media_m96::fuzzing::VARTFormat::RGB32_10:
      return VA_RT_FORMAT_RGB32_10;
    case media_m96::fuzzing::VARTFormat::PROTECTED:
      return VA_RT_FORMAT_PROTECTED;
  }
}

}  // namespace

namespace media_m96 {
namespace fuzzing {

// This wrapper lets us poke at the internals of a media_m96::VaapiJpegDecoder. It
// somewhat mirrors the way media_m96::VaapiImageDecoder drives a VaapiJpegDecoder.
class VaapiJpegDecoderWrapper {
 public:
  VaapiJpegDecoderWrapper() = default;
  ~VaapiJpegDecoderWrapper() = default;

  bool Initialize() { return decoder_.Initialize(base::DoNothing()); }

  bool MaybeCreateSurface(unsigned int picture_va_rt_format,
                          const gfx::Size& new_coded_size,
                          const gfx::Size& new_visible_size) {
    if (!decoder_.MaybeCreateSurface(picture_va_rt_format, new_coded_size,
                                     new_visible_size)) {
      decoder_.scoped_va_context_and_surface_.reset();
      return false;
    }
    return true;
  }

  bool SubmitBuffers(const media_m96::JpegParseResult& parse_result) {
    if (!decoder_.SubmitBuffers(parse_result)) {
      decoder_.scoped_va_context_and_surface_.reset();
      return false;
    }
    return true;
  }

  bool ExecuteAndDestroyPendingBuffers() {
    if (!decoder_.vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
            decoder_.scoped_va_context_and_surface_->id())) {
      decoder_.scoped_va_context_and_surface_.reset();
      return false;
    }
    return true;
  }

 private:
  VaapiJpegDecoder decoder_;
};

struct Environment {
  Environment() { VaapiWrapper::PreSandboxInitialization(); }
};

DEFINE_PROTO_FUZZER(const JpegImageList& image_list) {
  static const Environment env;
  VaapiJpegDecoderWrapper decoder_wrapper;
  if (!decoder_wrapper.Initialize()) {
    LOG(ERROR) << "Cannot initialize the VaapiJpegDecoder";
    abort();
  }
  for (const auto& jpeg_image : image_list.images()) {
    if (!decoder_wrapper.MaybeCreateSurface(
            ConvertToVARTFormat(jpeg_image.picture_va_rt_format()),
            gfx::Size(
                base::strict_cast<int>(jpeg_image.surface_coded_width()),
                base::strict_cast<int>(jpeg_image.surface_coded_height())),
            gfx::Size(
                base::strict_cast<int>(jpeg_image.surface_visible_width()),
                base::strict_cast<int>(jpeg_image.surface_visible_height())))) {
      continue;
    }
    if (!decoder_wrapper.SubmitBuffers(
            ConvertToJpegParseResult(jpeg_image.parse_result()))) {
      continue;
    }
    if (!decoder_wrapper.ExecuteAndDestroyPendingBuffers())
      continue;
    // TODO(crbug.com/1034357): for decodes that succeeded, it would be good to
    // get the result as a dma-buf, map it, and read it to make sure that doing
    // so does not lead us into an invalid state.
  }
}

}  // namespace fuzzing
}  // namespace media_m96
