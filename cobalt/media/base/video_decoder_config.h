// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define COBALT_MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/optional.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/size.h"
#include "cobalt/media/base/color_space.h"
#include "cobalt/media/base/encryption_scheme.h"
#include "cobalt/media/base/hdr_metadata.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/video_codecs.h"
#include "cobalt/media/base/video_types.h"
#include "cobalt/media/formats/webm/webm_colour_parser.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

MEDIA_EXPORT VideoCodec
VideoCodecProfileToVideoCodec(VideoCodecProfile profile);

class MEDIA_EXPORT VideoDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  VideoDecoderConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  VideoDecoderConfig(VideoCodec codec, VideoCodecProfile profile,
                     VideoPixelFormat format, ColorSpace color_space,
                     const math::Size& coded_size,
                     const math::Rect& visible_rect,
                     const math::Size& natural_size,
                     const std::vector<uint8_t>& extra_data,
                     const EncryptionScheme& encryption_scheme);

  ~VideoDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(VideoCodec codec, VideoCodecProfile profile,
                  VideoPixelFormat format, ColorSpace color_space,
                  const math::Size& coded_size, const math::Rect& visible_rect,
                  const math::Size& natural_size,
                  const std::vector<uint8_t>& extra_data,
                  const EncryptionScheme& encryption_scheme);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  // Returns true if all fields in |config| match this config.
  // Note: The contents of |extra_data_| are compared not the raw pointers.
  bool Matches(const VideoDecoderConfig& config) const;

  // Returns a human-readable string describing |*this|.  For debugging & test
  // output only.
  std::string AsHumanReadableString() const;

  std::string GetHumanReadableCodecName() const;

  static std::string GetHumanReadableProfile(VideoCodecProfile profile);

  VideoCodec codec() const { return codec_; }
  VideoCodecProfile profile() const { return profile_; }

  // Video format used to determine YUV buffer sizes.
  VideoPixelFormat format() const { return format_; }

  // The default color space of the decoded frames. Decoders should output
  // frames tagged with this color space unless they find a different value in
  // the bitstream.
  ColorSpace color_space() const { return color_space_; }

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  math::Size coded_size() const { return coded_size_; }

  // Region of |coded_size_| that is visible.
  math::Rect visible_rect() const { return visible_rect_; }

  // Final visible width and height of a video frame with aspect ratio taken
  // into account.
  math::Size natural_size() const { return natural_size_; }

  // Optional byte data required to initialize video decoders, such as H.264
  // AVCC data.
  const std::vector<uint8_t>& extra_data() const { return extra_data_; }

  // Whether the video stream is potentially encrypted.
  // Note that in a potentially encrypted video stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const { return encryption_scheme_.is_encrypted(); }

  // Encryption scheme used for encrypted buffers.
  const EncryptionScheme& encryption_scheme() const {
    return encryption_scheme_;
  }

  void set_webm_color_metadata(const WebMColorMetadata& webm_color_metadata) {
    webm_color_metadata_ = webm_color_metadata;
  }

  const WebMColorMetadata& webm_color_metadata() const {
    return webm_color_metadata_;
  }

  void set_mime(const std::string& mime) { mime_ = mime; }
  const std::string& mime() const { return mime_; }

 private:
  VideoCodec codec_;
  VideoCodecProfile profile_;

  VideoPixelFormat format_;

  // TODO(servolk): Deprecated, use color_space_info_ instead.
  ColorSpace color_space_;

  math::Size coded_size_;
  math::Rect visible_rect_;
  math::Size natural_size_;

  std::vector<uint8_t> extra_data_;

  EncryptionScheme encryption_scheme_;

  WebMColorMetadata webm_color_metadata_;

  // |mime_| contains the mime type string specified when creating the stream.
  // For example, when the stream is created via MediaSource, it is the mime
  // string passed to addSourceBuffer().  It can be an empty string when the
  // appropriate mime string is unknown.  It is provided as an extra information
  // and can be inconsistent with the other member variables.
  std::string mime_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
