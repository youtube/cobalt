// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "third_party/chromium/media/base/media_util.h"
#include "third_party/chromium/media/formats/mp2t/es_parser_mpeg1audio.h"

static void NewAudioConfig(const media_m96::AudioDecoderConfig& config) {}
static void EmitBuffer(scoped_refptr<media_m96::StreamParserBuffer> buffer) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media_m96::NullMediaLog media_log;
  media_m96::mp2t::EsParserMpeg1Audio es_parser(
      base::BindRepeating(&NewAudioConfig), base::BindRepeating(&EmitBuffer),
      &media_log);
  if (es_parser.Parse(data, size, media_m96::kNoTimestamp,
                      media_m96::kNoDecodeTimestamp())) {
    es_parser.Flush();
  }
  return 0;
}
