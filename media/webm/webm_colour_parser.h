// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_COLOUR_PARSER_H_
#define MEDIA_WEBM_WEBM_COLOUR_PARSER_H_

#include "base/compiler_specific.h"
#include "media/base/color_space.h"
#include "media/base/hdr_metadata.h"
#include "media/webm/webm_parser.h"

namespace media {

// WebM color information, containing HDR metadata:
// http://www.webmproject.org/docs/container/#Colour
struct MEDIA_EXPORT WebMColorMetadata {
  unsigned int BitsPerChannel;
  unsigned int ChromaSubsamplingHorz;
  unsigned int ChromaSubsamplingVert;
  unsigned int CbSubsamplingHorz;
  unsigned int CbSubsamplingVert;
  unsigned int ChromaSitingHorz;
  unsigned int ChromaSitingVert;

  gfx::ColorSpace color_space;

  HDRMetadata hdr_metadata;

  WebMColorMetadata();
  WebMColorMetadata(const WebMColorMetadata& rhs);
  bool operator==(const WebMColorMetadata& rhs) const;
};

// Parser for WebM MasteringMetadata within Colour element:
// http://www.webmproject.org/docs/container/#MasteringMetadata
class WebMMasteringMetadataParser : public WebMParserClient {
 public:
  WebMMasteringMetadataParser();
  ~WebMMasteringMetadataParser() override;

  MasteringMetadata GetMasteringMetadata() const { return mastering_metadata_; }

 private:
  // WebMParserClient implementation.
  bool OnFloat(int id, double val) override;

  MasteringMetadata mastering_metadata_;
  DISALLOW_COPY_AND_ASSIGN(WebMMasteringMetadataParser);
};

// Parser for WebM Colour element:
// http://www.webmproject.org/docs/container/#colour
class WebMColourParser : public WebMParserClient {
 public:
  WebMColourParser();
  ~WebMColourParser() override;

  void Reset();

  WebMColorMetadata GetWebMColorMetadata() const;

 private:
  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;

  int64_t matrix_coefficients_;
  int64_t bits_per_channel_;
  int64_t chroma_subsampling_horz_;
  int64_t chroma_subsampling_vert_;
  int64_t cb_subsampling_horz_;
  int64_t cb_subsampling_vert_;
  int64_t chroma_siting_horz_;
  int64_t chroma_siting_vert_;
  int64_t range_;
  int64_t transfer_characteristics_;
  int64_t primaries_;
  int64_t max_cll_;
  int64_t max_fall_;

  WebMMasteringMetadataParser mastering_metadata_parser_;
  bool mastering_metadata_parsed_;

  DISALLOW_COPY_AND_ASSIGN(WebMColourParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_COLOUR_PARSER_H_
