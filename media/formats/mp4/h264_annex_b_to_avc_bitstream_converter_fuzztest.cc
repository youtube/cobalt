// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  std::vector<uint8_t> output(size);
  size_t size_out;
  bool config_changed;
  media::H264AnnexBToAvcBitstreamConverter converter;
  base::span<const uint8_t> input(data, data + size);

  auto status =
      converter.ConvertChunk(input, output, &config_changed, &size_out);

  auto& config = converter.GetCurrentConfig();

  std::vector<uint8_t> avc_config(size);
  config.Serialize(avc_config);

  return 0;
}
