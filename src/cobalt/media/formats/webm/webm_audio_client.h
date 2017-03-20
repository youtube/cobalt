// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_
#define COBALT_MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/formats/webm/webm_parser.h"

namespace media {
class AudioDecoderConfig;
class EncryptionScheme;

// Helper class used to parse an Audio element inside a TrackEntry element.
class WebMAudioClient : public WebMParserClient {
 public:
  explicit WebMAudioClient(const scoped_refptr<MediaLog>& media_log);
  ~WebMAudioClient() OVERRIDE;

  // Reset this object's state so it can process a new audio track element.
  void Reset();

  // Initialize |config| with the data in |codec_id|, |codec_private|,
  // |encryption_scheme| and the fields parsed from the last audio track element
  // this object was used to parse.
  // Returns true if |config| was successfully initialized.
  // Returns false if there was unexpected values in the provided parameters or
  // audio track element fields.
  bool InitializeConfig(const std::string& codec_id,
                        const std::vector<uint8_t>& codec_private,
                        const int64_t seek_preroll, const int64_t codec_delay,
                        const EncryptionScheme& encryption_scheme,
                        AudioDecoderConfig* config);

 private:
  // WebMParserClient implementation.
  bool OnUInt(int id, int64_t val) OVERRIDE;
  bool OnFloat(int id, double val) OVERRIDE;

  scoped_refptr<MediaLog> media_log_;
  int channels_;
  double samples_per_second_;
  double output_samples_per_second_;

  DISALLOW_COPY_AND_ASSIGN(WebMAudioClient);
};

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_
