// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_WEBM_VIDEO_CLIENT_H_
#define COBALT_MEDIA_FORMATS_WEBM_WEBM_VIDEO_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/formats/webm/webm_colour_parser.h"
#include "cobalt/media/formats/webm/webm_parser.h"

namespace media {
class EncryptionScheme;
class VideoDecoderConfig;

// Helper class used to parse a Video element inside a TrackEntry element.
class WebMVideoClient : public WebMParserClient {
 public:
  explicit WebMVideoClient(const scoped_refptr<MediaLog>& media_log);
  ~WebMVideoClient() OVERRIDE;

  // Reset this object's state so it can process a new video track element.
  void Reset();

  // Initialize |config| with the data in |codec_id|, |codec_private|,
  // |encryption_scheme| and the fields parsed from the last video track element
  // this object was used to parse.
  // Returns true if |config| was successfully initialized.
  // Returns false if there was unexpected values in the provided parameters or
  // video track element fields. The contents of |config| are undefined in this
  // case and should not be relied upon.
  bool InitializeConfig(const std::string& codec_id,
                        const std::vector<uint8_t>& codec_private,
                        const EncryptionScheme& encryption_scheme,
                        VideoDecoderConfig* config);

 private:
  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) OVERRIDE;
  bool OnListEnd(int id) OVERRIDE;
  bool OnUInt(int id, int64_t val) OVERRIDE;
  bool OnBinary(int id, const uint8_t* data, int size) OVERRIDE;
  bool OnFloat(int id, double val) OVERRIDE;

  scoped_refptr<MediaLog> media_log_;
  int64_t pixel_width_;
  int64_t pixel_height_;
  int64_t crop_bottom_;
  int64_t crop_top_;
  int64_t crop_left_;
  int64_t crop_right_;
  int64_t display_width_;
  int64_t display_height_;
  int64_t display_unit_;
  int64_t alpha_mode_;

  WebMColourParser colour_parser_;
  bool colour_parsed_;

  DISALLOW_COPY_AND_ASSIGN(WebMVideoClient);
};

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_WEBM_VIDEO_CLIENT_H_
