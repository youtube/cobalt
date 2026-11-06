// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/splash_video_webm_indexer.h"

#include <string>
#include <utility>

#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "media/formats/webm/webm_constants.h"
#include "media/formats/webm/webm_parser.h"

namespace content {

namespace {

constexpr char kVp9CodecId[] = "V_VP9";

// The TrackType of video tracks, per the Matroska specification.
constexpr int64_t kVideoTrackType = 1;

// The TimecodeScale when Info doesn't set it: 1ms.
constexpr int64_t kDefaultTimecodeScaleNs = 1'000'000;

// Bits of the flags byte of a SimpleBlock or Block.
constexpr uint8_t kKeyFrameFlag = 0x80;  // SimpleBlock only.
constexpr uint8_t kLacingFlags = 0x06;

// Every VP9 frame, and every superframe, starts with the 2-bit frame marker
// 0b10, then the low and high bits of the profile: 0 for 8-bit 4:2:0, the only
// format that the player is configured for.
bool IsVp9Profile0Frame(uint8_t first_byte) {
  return (first_byte & 0xF0) == 0x80;
}

// Parses the header of a SimpleBlock or Block: the track number (an EBML
// variable-length integer), a signed 16-bit timecode relative to the Cluster's
// and the flags.
bool ParseBlockHeader(const uint8_t* data,
                      int size,
                      int64_t* track_number,
                      int16_t* relative_timecode,
                      uint8_t* flags,
                      int* header_size) {
  if (size < 1) {
    return false;
  }
  // The length of the track number is 1 + the number of leading zero bits of
  // its first byte.
  int length = 1;
  uint8_t marker = 0x80;
  while (length <= 8 && !(data[0] & marker)) {
    marker >>= 1;
    ++length;
  }
  if (length > 8 || size < length + 3) {
    return false;
  }
  int64_t value = data[0] & (marker - 1);
  for (int i = 1; i < length; ++i) {
    value = (value << 8) | data[i];
  }
  *track_number = value;
  *relative_timecode =
      static_cast<int16_t>((data[length] << 8) | data[length + 1]);
  *flags = data[length + 2];
  *header_size = length + 3;
  return true;
}

// Collects the frames and track properties of a splash screen video from the
// elements of Info, Tracks and Clusters, as parsed by media::WebMListParser.
class SplashVideoWebMClient : public media::WebMParserClient {
 public:
  explicit SplashVideoWebMClient(base::span<const uint8_t> webm)
      : webm_(webm) {}

  SplashVideoWebMClient(const SplashVideoWebMClient&) = delete;
  SplashVideoWebMClient& operator=(const SplashVideoWebMClient&) = delete;

  ~SplashVideoWebMClient() override = default;

  // Returns the index of the frames collected so far, or std::nullopt if they
  // don't make a valid splash screen video.
  std::optional<SplashVideoIndex> TakeIndex() {
    if (track_count_ != 1 || codec_id_ != kVp9CodecId ||
        track_type_ != kVideoTrackType || !track_number_) {
      return std::nullopt;
    }
    if (width_ <= 0 || height_ <= 0 || width_ > kMaxSplashVideoDimension ||
        height_ > kMaxSplashVideoDimension || timecode_scale_ns_ <= 0) {
      return std::nullopt;
    }
    if (frames_.empty() || !frames_.front().is_key_frame) {
      return std::nullopt;
    }

    SplashVideoIndex index;
    index.width = static_cast<int>(width_);
    index.height = static_cast<int>(height_);
    index.frames.reserve(frames_.size());
    int64_t previous_timestamp_us = -1;
    for (const Frame& frame : frames_) {
      if (frame.track_number != *track_number_ ||
          !IsVp9Profile0Frame(webm_[frame.offset])) {
        return std::nullopt;
      }
      int64_t timestamp_ns = 0;
      if (!base::CheckMul(frame.timecode, timecode_scale_ns_)
               .AssignIfValid(&timestamp_ns)) {
        return std::nullopt;
      }
      const int64_t timestamp_us = timestamp_ns / 1000;
      // The video starts at 0, where the player seeks to start it.
      const bool is_first_frame = index.frames.empty();
      if ((is_first_frame && timestamp_us != 0) ||
          (!is_first_frame && timestamp_us <= previous_timestamp_us) ||
          timestamp_us > kMaxSplashVideoDurationUs) {
        return std::nullopt;
      }
      previous_timestamp_us = timestamp_us;
      index.frames.push_back(
          {frame.offset, frame.size, timestamp_us, frame.is_key_frame});
    }
    return index;
  }

  // media::WebMParserClient:
  using media::WebMParserClient::OnBinary;

  media::WebMParserClient* OnListStart(int id) override {
    switch (id) {
      case media::kWebMIdTrackEntry:
        ++track_count_;
        break;
      case media::kWebMIdCluster:
        cluster_timecode_.reset();
        break;
      case media::kWebMIdBlockGroup:
        block_.reset();
        block_has_reference_ = false;
        break;
    }
    return this;
  }

  bool OnListEnd(int id) override {
    if (id != media::kWebMIdBlockGroup) {
      return true;
    }
    if (!block_) {
      return false;
    }
    // A Block without a ReferenceBlock is a key frame.
    block_->is_key_frame = !block_has_reference_;
    const Frame frame = *block_;
    block_.reset();
    return AddFrame(frame);
  }

  bool OnUInt(int id, int64_t val) override {
    switch (id) {
      case media::kWebMIdTimecodeScale:
        timecode_scale_ns_ = val;
        break;
      case media::kWebMIdTrackNumber:
        track_number_ = val;
        break;
      case media::kWebMIdTrackType:
        track_type_ = val;
        break;
      case media::kWebMIdPixelWidth:
        width_ = val;
        break;
      case media::kWebMIdPixelHeight:
        height_ = val;
        break;
      case media::kWebMIdTimecode:
        cluster_timecode_ = val;
        break;
    }
    return true;
  }

  bool OnFloat(int id, double val) override { return true; }

  bool OnBinary(int id, const uint8_t* data, int size) override {
    switch (id) {
      case media::kWebMIdSimpleBlock: {
        std::optional<Frame> frame =
            ParseBlock(data, size, /*is_simple_block=*/true);
        return frame && AddFrame(*frame);
      }
      case media::kWebMIdBlock:
        // A BlockGroup holds a single Block.
        if (block_) {
          return false;
        }
        block_ = ParseBlock(data, size, /*is_simple_block=*/false);
        return block_.has_value();
      case media::kWebMIdReferenceBlock:
        block_has_reference_ = true;
        break;
    }
    return true;
  }

  bool OnString(int id, const std::string& str) override {
    if (id == media::kWebMIdCodecID) {
      codec_id_ = str;
    }
    return true;
  }

 private:
  struct Frame {
    int64_t track_number = 0;
    size_t offset = 0;
    size_t size = 0;
    // In units of TimecodeScale, which may only be known once parsing is done.
    int64_t timecode = 0;
    bool is_key_frame = false;
  };

  std::optional<Frame> ParseBlock(const uint8_t* data,
                                  int size,
                                  bool is_simple_block) {
    // Blocks follow the Timecode of their Cluster.
    if (!cluster_timecode_) {
      return std::nullopt;
    }
    int64_t track_number = 0;
    int16_t relative_timecode = 0;
    uint8_t flags = 0;
    int header_size = 0;
    if (!ParseBlockHeader(data, size, &track_number, &relative_timecode, &flags,
                          &header_size)) {
      return std::nullopt;
    }
    // A laced block holds several frames, which video doesn't use.
    if (flags & kLacingFlags) {
      return std::nullopt;
    }
    const uint8_t* frame_data = data + header_size;
    const int frame_size = size - header_size;
    // WebMListParser passes pointers into the buffer it parses, i.e. `webm_`.
    if (frame_size <= 0 || frame_data < webm_.data() ||
        frame_data + frame_size > webm_.data() + webm_.size()) {
      return std::nullopt;
    }
    int64_t timecode = 0;
    if (!base::CheckAdd(*cluster_timecode_, relative_timecode)
             .AssignIfValid(&timecode)) {
      return std::nullopt;
    }
    Frame frame;
    frame.track_number = track_number;
    frame.offset = static_cast<size_t>(frame_data - webm_.data());
    frame.size = static_cast<size_t>(frame_size);
    frame.timecode = timecode;
    frame.is_key_frame = is_simple_block && (flags & kKeyFrameFlag);
    return frame;
  }

  bool AddFrame(const Frame& frame) {
    if (frames_.size() >= kMaxSplashVideoFrames) {
      return false;
    }
    frames_.push_back(frame);
    return true;
  }

  const base::span<const uint8_t> webm_;

  int track_count_ = 0;
  std::optional<int64_t> track_number_;
  int64_t track_type_ = 0;
  std::string codec_id_;
  int64_t width_ = 0;
  int64_t height_ = 0;
  int64_t timecode_scale_ns_ = kDefaultTimecodeScaleNs;

  std::optional<int64_t> cluster_timecode_;
  std::optional<Frame> block_;
  bool block_has_reference_ = false;

  std::vector<Frame> frames_;
};

}  // namespace

SplashVideoIndex::SplashVideoIndex() = default;
SplashVideoIndex::SplashVideoIndex(SplashVideoIndex&&) = default;
SplashVideoIndex& SplashVideoIndex::operator=(SplashVideoIndex&&) = default;
SplashVideoIndex::~SplashVideoIndex() = default;

std::optional<SplashVideoIndex> IndexSplashVideoWebM(
    base::span<const uint8_t> webm,
    size_t max_file_size) {
  if (webm.empty() || webm.size() > max_file_size) {
    return std::nullopt;
  }
  const uint8_t* const data = webm.data();
  const int size = base::checked_cast<int>(webm.size());

  // The file starts with the EBML header, whose contents aren't needed.
  int id = 0;
  int64_t element_size = 0;
  int header_size =
      media::WebMParseElementHeader(data, size, &id, &element_size);
  if (header_size <= 0 || id != media::kWebMIdEBMLHeader || element_size < 0 ||
      element_size > size - header_size) {
    return std::nullopt;
  }
  int pos = header_size + static_cast<int>(element_size);

  // Then comes the Segment, which holds everything else.
  header_size =
      media::WebMParseElementHeader(data + pos, size - pos, &id, &element_size);
  if (header_size <= 0 || id != media::kWebMIdSegment) {
    return std::nullopt;
  }
  pos += header_size;
  int segment_end = size;
  if (element_size != media::kWebMUnknownSize) {
    if (element_size < 0 || element_size > size - pos) {
      return std::nullopt;
    }
    segment_end = pos + static_cast<int>(element_size);
  }

  // Only Info, Tracks and Clusters are parsed. The other top-level elements,
  // e.g., SeekHead, Void, Tags and Cues, are skipped.
  SplashVideoWebMClient client(webm);
  while (pos < segment_end) {
    header_size = media::WebMParseElementHeader(data + pos, segment_end - pos,
                                                &id, &element_size);
    if (header_size <= 0 || element_size == media::kWebMUnknownSize ||
        element_size < 0 || element_size > segment_end - pos - header_size) {
      return std::nullopt;
    }
    const int element_end = pos + header_size + static_cast<int>(element_size);
    if (id == media::kWebMIdInfo || id == media::kWebMIdTracks ||
        id == media::kWebMIdCluster) {
      media::WebMListParser parser(id, &client);
      const int element_total_size = element_end - pos;
      if (parser.Parse(data + pos, element_total_size) != element_total_size ||
          !parser.IsParsingComplete()) {
        return std::nullopt;
      }
    }
    pos = element_end;
  }
  return client.TakeIndex();
}

}  // namespace content
