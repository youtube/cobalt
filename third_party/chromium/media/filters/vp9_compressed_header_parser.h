// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VP9_COMPRESSED_HEADER_PARSER_H_
#define MEDIA_FILTERS_VP9_COMPRESSED_HEADER_PARSER_H_

#include "media/filters/vp9_bool_decoder.h"
#include "media/filters/vp9_parser.h"

namespace media {

class Vp9CompressedHeaderParser {
 public:
  Vp9CompressedHeaderParser();

  // Parses VP9 compressed header in |stream| with |frame_size| into |fhdr|.
  // Returns true if no error.
  bool Parse(const uint8_t* stream, off_t frame_size, Vp9FrameHeader* fhdr);

 private:
  void ReadTxMode(Vp9FrameHeader* fhdr);
  uint8_t DecodeTermSubexp();
  void DiffUpdateProb(Vp9Prob* prob);
  template <int N>
  void DiffUpdateProbArray(Vp9Prob (&prob_array)[N]);
  void ReadTxModeProbs(Vp9FrameContext* frame_context);
  void ReadCoefProbs(Vp9FrameHeader* fhdr);
  void ReadSkipProb(Vp9FrameContext* frame_context);
  void ReadInterModeProbs(Vp9FrameContext* frame_context);
  void ReadInterpFilterProbs(Vp9FrameContext* frame_context);
  void ReadIsInterProbs(Vp9FrameContext* frame_context);
  void ReadFrameReferenceMode(Vp9FrameHeader* fhdr);
  void ReadFrameReferenceModeProbs(Vp9FrameHeader* fhdr);
  void ReadYModeProbs(Vp9FrameContext* frame_context);
  void ReadPartitionProbs(Vp9FrameContext* frame_context);
  void ReadMvProbs(bool allow_high_precision_mv,
                   Vp9FrameContext* frame_context);
  void UpdateMvProb(Vp9Prob* prob);
  template <int N>
  void UpdateMvProbArray(Vp9Prob (&prob_array)[N]);

  // Bool decoder for compressed frame header.
  Vp9BoolDecoder reader_;

  DISALLOW_COPY_AND_ASSIGN(Vp9CompressedHeaderParser);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VP9_COMPRESSED_HEADER_PARSER_H_
