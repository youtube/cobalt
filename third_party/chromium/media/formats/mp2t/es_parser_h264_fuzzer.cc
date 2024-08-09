// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "third_party/chromium/media/formats/mp2t/es_parser_h264.h"

static void NewVideoConfig(const media_m96::VideoDecoderConfig& config) {}
static void EmitBuffer(scoped_refptr<media_m96::StreamParserBuffer> buffer) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media_m96::mp2t::EsParserH264 es_parser(base::BindRepeating(&NewVideoConfig),
                                      base::BindRepeating(&EmitBuffer));
  if (!es_parser.Parse(data, size, media_m96::kNoTimestamp,
                       media_m96::kNoDecodeTimestamp())) {
    return 0;
  }
  return 0;
}
