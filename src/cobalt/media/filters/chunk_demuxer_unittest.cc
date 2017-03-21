// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/filters/chunk_demuxer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <queue>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/decrypt_config.h"
#include "cobalt/media/base/media.h"
#include "cobalt/media/base/media_tracks.h"
#include "cobalt/media/base/mock_demuxer_host.h"
#include "cobalt/media/base/mock_media_log.h"
#include "cobalt/media/base/test_data_util.h"
#include "cobalt/media/base/test_helpers.h"
#include "cobalt/media/base/timestamp_constants.h"
#include "cobalt/media/formats/webm/cluster_builder.h"
#include "cobalt/media/formats/webm/webm_cluster_parser.h"
#include "cobalt/media/formats/webm/webm_constants.h"
#include "cobalt/media/media_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Exactly;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

const uint8_t kTracksHeader[] = {
    0x16, 0x54, 0xAE, 0x6B,                          // Tracks ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // tracks(size = 0)
};

// WebM Block bytes that represent a VP8 key frame.
const uint8_t kVP8Keyframe[] = {0x010, 0x00, 0x00, 0x9d, 0x01, 0x2a,
                                0x00,  0x10, 0x00, 0x10, 0x00};

// WebM Block bytes that represent a VP8 interframe.
const uint8_t kVP8Interframe[] = {0x11, 0x00, 0x00};

const uint8_t kCuesHeader[] = {
    0x1C, 0x53, 0xBB, 0x6B,                          // Cues ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // cues(size = 0)
};

const uint8_t kEncryptedMediaInitData[] = {
    0x68, 0xFE, 0xF9, 0xA1, 0xB3, 0x0D, 0x6B, 0x4D,
    0xF2, 0x22, 0xB5, 0x0B, 0x4D, 0xE9, 0xE9, 0x95,
};

const int kTracksHeaderSize = sizeof(kTracksHeader);
const int kTracksSizeOffset = 4;

// The size of TrackEntry element in test file "webm_vorbis_track_entry" starts
// at index 1 and spans 8 bytes.
const int kAudioTrackSizeOffset = 1;
const int kAudioTrackSizeWidth = 8;
const int kAudioTrackEntryHeaderSize =
    kAudioTrackSizeOffset + kAudioTrackSizeWidth;

// The size of TrackEntry element in test file "webm_vp8_track_entry" starts at
// index 1 and spans 8 bytes.
const int kVideoTrackSizeOffset = 1;
const int kVideoTrackSizeWidth = 8;
const int kVideoTrackEntryHeaderSize =
    kVideoTrackSizeOffset + kVideoTrackSizeWidth;

const int kVideoTrackNum = 1;
const int kAudioTrackNum = 2;
const int kTextTrackNum = 3;
const int kAlternateVideoTrackNum = 4;
const int kAlternateAudioTrackNum = 5;
const int kAlternateTextTrackNum = 6;

const int kAudioBlockDuration = 23;
const int kVideoBlockDuration = 33;
const int kTextBlockDuration = 100;
const int kBlockSize = 10;

const char kSourceId[] = "SourceId";
const char kDefaultFirstClusterRange[] = "{ [0,46) }";
const int kDefaultFirstClusterEndTimestamp = 66;
const int kDefaultSecondClusterEndTimestamp = 132;

base::TimeDelta kDefaultDuration() {
  return base::TimeDelta::FromMilliseconds(201224);
}

// Write an integer into buffer in the form of vint that spans 8 bytes.
// The data pointed by |buffer| should be at least 8 bytes long.
// |number| should be in the range 0 <= number < 0x00FFFFFFFFFFFFFF.
static void WriteInt64(uint8_t* buffer, int64_t number) {
  DCHECK(number >= 0 && number < 0x00FFFFFFFFFFFFFFLL);
  buffer[0] = 0x01;
  int64_t tmp = number;
  for (int i = 7; i > 0; i--) {
    buffer[i] = tmp & 0xff;
    tmp >>= 8;
  }
}

static void OnReadDone(const base::TimeDelta& expected_time, bool* called,
                       DemuxerStream::Status status,
                       const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);
  EXPECT_EQ(expected_time, buffer->timestamp());
  *called = true;
}

static void OnReadDone_AbortExpected(
    bool* called, DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kAborted);
  EXPECT_EQ(NULL, buffer.get());
  *called = true;
}

static void OnReadDone_EOSExpected(bool* called, DemuxerStream::Status status,
                                   const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);
  EXPECT_TRUE(buffer->end_of_stream());
  *called = true;
}

static void OnSeekDone_OKExpected(bool* called, PipelineStatus status) {
  EXPECT_EQ(status, PIPELINE_OK);
  *called = true;
}

class ChunkDemuxerTest : public ::testing::Test {
 public:
  // Public method because test cases use it directly.
  MOCK_METHOD1(DemuxerInitialized, void(PipelineStatus));

 protected:
  enum CodecsIndex { AUDIO, VIDEO, MAX_CODECS_INDEX };

  // Default cluster to append first for simple tests.
  std::unique_ptr<Cluster> kDefaultFirstCluster() {
    return GenerateCluster(0, 4);
  }

  // Default cluster to append after kDefaultFirstCluster()
  // has been appended. This cluster starts with blocks that
  // have timestamps consistent with the end times of the blocks
  // in kDefaultFirstCluster() so that these two clusters represent
  // a continuous region.
  std::unique_ptr<Cluster> kDefaultSecondCluster() {
    return GenerateCluster(46, 66, 5);
  }

  ChunkDemuxerTest()
      : media_log_(new StrictMock<MockMediaLog>()),
        append_window_end_for_next_append_(kInfiniteDuration) {
    init_segment_received_cb_ = base::Bind(
        &ChunkDemuxerTest::InitSegmentReceived, base::Unretained(this));
    CreateNewDemuxer();
  }

  void CreateNewDemuxer() {
    base::Closure open_cb =
        base::Bind(&ChunkDemuxerTest::DemuxerOpened, base::Unretained(this));
    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb = base::Bind(
        &ChunkDemuxerTest::OnEncryptedMediaInitData, base::Unretained(this));
    demuxer_.reset(new ChunkDemuxer(open_cb, encrypted_media_init_data_cb,
                                    media_log_, true));
  }

  virtual ~ChunkDemuxerTest() { ShutdownDemuxer(); }

  void CreateInitSegment(int stream_flags, bool is_audio_encrypted,
                         bool is_video_encrypted,
                         std::unique_ptr<uint8_t[]>* buffer, int* size) {
    bool has_audio = (stream_flags & HAS_AUDIO) != 0;
    bool has_video = (stream_flags & HAS_VIDEO) != 0;
    bool has_text = (stream_flags & HAS_TEXT) != 0;
    scoped_refptr<DecoderBuffer> ebml_header;
    scoped_refptr<DecoderBuffer> info;
    scoped_refptr<DecoderBuffer> audio_track_entry;
    scoped_refptr<DecoderBuffer> video_track_entry;
    scoped_refptr<DecoderBuffer> audio_content_encodings;
    scoped_refptr<DecoderBuffer> video_content_encodings;
    scoped_refptr<DecoderBuffer> text_track_entry;

    ebml_header = ReadTestDataFile("webm_ebml_element");

    info = ReadTestDataFile("webm_info_element");

    int tracks_element_size = 0;

    if (has_audio) {
      audio_track_entry = ReadTestDataFile("webm_vorbis_track_entry");
      tracks_element_size += audio_track_entry->data_size();
      // Verify that we have TrackNum (0xD7) EBML element at expected offset.
      DCHECK_EQ(audio_track_entry->data()[9], kWebMIdTrackNumber);
      // Verify that the size of TrackNum element is 1. The actual value is 0x81
      // due to how element sizes are encoded in EBML.
      DCHECK_EQ(audio_track_entry->data()[10], 0x81);
      // Ensure the track id in TrackNum EBML element matches kAudioTrackNum.
      DCHECK_EQ(audio_track_entry->data()[11], kAudioTrackNum);
      if (stream_flags & USE_ALTERNATE_AUDIO_TRACK_ID)
        audio_track_entry->writable_data()[11] = kAlternateAudioTrackNum;
      if (is_audio_encrypted) {
        audio_content_encodings = ReadTestDataFile("webm_content_encodings");
        tracks_element_size += audio_content_encodings->data_size();
      }
    }

    if (has_video) {
      video_track_entry = ReadTestDataFile("webm_vp8_track_entry");
      tracks_element_size += video_track_entry->data_size();
      // Verify that we have TrackNum (0xD7) EBML element at expected offset.
      DCHECK_EQ(video_track_entry->data()[9], kWebMIdTrackNumber);
      // Verify that the size of TrackNum element is 1. The actual value is 0x81
      // due to how element sizes are encoded in EBML.
      DCHECK_EQ(video_track_entry->data()[10], 0x81);
      // Ensure the track id in TrackNum EBML element matches kVideoTrackNum.
      DCHECK_EQ(video_track_entry->data()[11], kVideoTrackNum);
      if (stream_flags & USE_ALTERNATE_VIDEO_TRACK_ID)
        video_track_entry->writable_data()[11] = kAlternateVideoTrackNum;
      if (is_video_encrypted) {
        video_content_encodings = ReadTestDataFile("webm_content_encodings");
        tracks_element_size += video_content_encodings->data_size();
      }
    }

    if (has_text) {
      // TODO(matthewjheaney): create an abstraction to do
      // this (http://crbug/321454).
      // We need it to also handle the creation of multiple text tracks.
      //
      // This is the track entry for a text track,
      // TrackEntry [AE], size=30
      //   TrackNum [D7], size=1, val=3 (or 4 if USE_ALTERNATE_TEXT_TRACK_ID)
      //   TrackUID [73] [C5], size=1, value=3 (must remain constant for same
      //     track, even if TrackNum changes)
      //   TrackType [83], size=1, val=0x11
      //   CodecId [86], size=18, val="D_WEBVTT/SUBTITLES"
      char str[] =
          "\xAE\x9E\xD7\x81\x03\x73\xC5\x81\x03"
          "\x83\x81\x11\x86\x92"
          "D_WEBVTT/SUBTITLES";
      DCHECK_EQ(str[4], kTextTrackNum);
      if (stream_flags & USE_ALTERNATE_TEXT_TRACK_ID)
        str[4] = kAlternateTextTrackNum;

      const int len = strlen(str);
      DCHECK_EQ(len, 32);
      const uint8_t* const buf = reinterpret_cast<const uint8_t*>(str);
      text_track_entry = DecoderBuffer::CopyFrom(buf, len);
      tracks_element_size += text_track_entry->data_size();
    }

    *size = ebml_header->data_size() + info->data_size() + kTracksHeaderSize +
            tracks_element_size;

    buffer->reset(new uint8_t[*size]);

    uint8_t* buf = buffer->get();
    memcpy(buf, ebml_header->data(), ebml_header->data_size());
    buf += ebml_header->data_size();

    memcpy(buf, info->data(), info->data_size());
    buf += info->data_size();

    memcpy(buf, kTracksHeader, kTracksHeaderSize);
    WriteInt64(buf + kTracksSizeOffset, tracks_element_size);
    buf += kTracksHeaderSize;

    // TODO(xhwang): Simplify this! Probably have test data files that contain
    // ContentEncodings directly instead of trying to create one at run-time.
    if (has_video) {
      memcpy(buf, video_track_entry->data(), video_track_entry->data_size());
      if (is_video_encrypted) {
        memcpy(buf + video_track_entry->data_size(),
               video_content_encodings->data(),
               video_content_encodings->data_size());
        WriteInt64(buf + kVideoTrackSizeOffset,
                   video_track_entry->data_size() +
                       video_content_encodings->data_size() -
                       kVideoTrackEntryHeaderSize);
        buf += video_content_encodings->data_size();
      }
      buf += video_track_entry->data_size();
    }

    if (has_audio) {
      memcpy(buf, audio_track_entry->data(), audio_track_entry->data_size());
      if (is_audio_encrypted) {
        memcpy(buf + audio_track_entry->data_size(),
               audio_content_encodings->data(),
               audio_content_encodings->data_size());
        WriteInt64(buf + kAudioTrackSizeOffset,
                   audio_track_entry->data_size() +
                       audio_content_encodings->data_size() -
                       kAudioTrackEntryHeaderSize);
        buf += audio_content_encodings->data_size();
      }
      buf += audio_track_entry->data_size();
    }

    if (has_text) {
      memcpy(buf, text_track_entry->data(), text_track_entry->data_size());
      buf += text_track_entry->data_size();
    }
  }

  ChunkDemuxer::Status AddId() {
    return AddId(kSourceId, HAS_AUDIO | HAS_VIDEO);
  }

  ChunkDemuxer::Status AddId(const std::string& source_id, int stream_flags) {
    bool has_audio = (stream_flags & HAS_AUDIO) != 0;
    bool has_video = (stream_flags & HAS_VIDEO) != 0;
    std::string codecs;
    std::string type;

    if (has_audio) {
      codecs += "vorbis";
      type = "audio/webm";
    }

    if (has_video) {
      if (codecs == "")
        codecs = "vp8";
      else
        codecs += ",vp8";
      type = "video/webm";
    }

    if (!has_audio && !has_video) {
      return AddId(kSourceId, HAS_AUDIO | HAS_VIDEO);
    }

    return AddId(source_id, type, codecs);
  }

  ChunkDemuxer::Status AddId(const std::string& source_id,
                             const std::string& mime_type,
                             const std::string& codecs) {
    ChunkDemuxer::Status status = demuxer_->AddId(source_id, mime_type, codecs);
    if (status == ChunkDemuxer::kOk)
      demuxer_->SetTracksWatcher(
          source_id, base::Bind(&ChunkDemuxerTest::InitSegmentReceived,
                                base::Unretained(this)));
    return status;
  }

  bool AppendData(const uint8_t* data, size_t length) {
    return AppendData(kSourceId, data, length);
  }

  bool AppendCluster(const std::string& source_id,
                     std::unique_ptr<Cluster> cluster) {
    return AppendData(source_id, cluster->data(), cluster->size());
  }

  bool AppendCluster(std::unique_ptr<Cluster> cluster) {
    return AppendCluster(kSourceId, std::move(cluster));
  }

  bool AppendCluster(int timecode, int block_count) {
    return AppendCluster(GenerateCluster(timecode, block_count));
  }

  void AppendSingleStreamCluster(const std::string& source_id, int track_number,
                                 int timecode, int block_count) {
    int block_duration = 0;
    switch (track_number) {
      case kVideoTrackNum:
      case kAlternateVideoTrackNum:
        block_duration = kVideoBlockDuration;
        break;
      case kAudioTrackNum:
      case kAlternateAudioTrackNum:
        block_duration = kAudioBlockDuration;
        break;
      case kTextTrackNum:  // Fall-through.
      case kAlternateTextTrackNum:
        block_duration = kTextBlockDuration;
        break;
    }
    ASSERT_NE(block_duration, 0);
    int end_timecode = timecode + block_count * block_duration;
    ASSERT_TRUE(AppendCluster(
        source_id, GenerateSingleStreamCluster(timecode, end_timecode,
                                               track_number, block_duration)));
  }

  struct BlockInfo {
    BlockInfo() : track_number(0), timestamp_in_ms(0), flags(0), duration(0) {}

    BlockInfo(int tn, int ts, int f, int d)
        : track_number(tn), timestamp_in_ms(ts), flags(f), duration(d) {}

    int track_number;
    int timestamp_in_ms;
    int flags;
    int duration;

    bool operator<(const BlockInfo& rhs) const {
      return timestamp_in_ms < rhs.timestamp_in_ms;
    }
  };

  // |track_number| - The track number to place in
  // |block_descriptions| - A space delimited string of block info that
  //  is used to populate |blocks|. Each block info has a timestamp in
  //  milliseconds and optionally followed by a 'K' to indicate that a block
  //  should be marked as a key frame. For example "0K 30 60" should populate
  //  |blocks| with 3 BlockInfo objects: a key frame with timestamp 0 and 2
  //  non-key-frames at 30ms and 60ms.
  //  Every block will be a SimpleBlock, with the exception that the last block
  //  may have an optional duration delimited with a 'D' and appended to the
  //  block info timestamp, prior to the optional keyframe 'K'. For example "0K
  //  30 60D10K" indicates that the last block will be a keyframe BlockGroup
  //  with duration 10ms.
  void ParseBlockDescriptions(int track_number,
                              const std::string block_descriptions,
                              std::vector<BlockInfo>* blocks) {
    std::vector<std::string> timestamps = base::SplitString(
        block_descriptions, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    for (size_t i = 0; i < timestamps.size(); ++i) {
      std::string timestamp_str = timestamps[i];
      BlockInfo block_info;
      block_info.track_number = track_number;
      block_info.flags = 0;
      block_info.duration = 0;

      if (base::EndsWith(timestamp_str, "K", base::CompareCase::SENSITIVE)) {
        block_info.flags = kWebMFlagKeyframe;
        // Remove the "K" off of the token.
        timestamp_str = timestamp_str.substr(0, timestamps[i].length() - 1);
      }

      size_t duration_pos = timestamp_str.find('D');
      const bool explicit_duration = duration_pos != std::string::npos;
      const bool is_last_block = i == timestamps.size() - 1;
      CHECK(!explicit_duration || is_last_block);
      if (explicit_duration) {
        CHECK(base::StringToInt(timestamp_str.substr(duration_pos + 1),
                                &block_info.duration));
        timestamp_str = timestamp_str.substr(0, duration_pos);
      }

      CHECK(base::StringToInt(timestamp_str, &block_info.timestamp_in_ms));

      if (track_number == kTextTrackNum ||
          track_number == kAlternateTextTrackNum) {
        block_info.duration = kTextBlockDuration;
        ASSERT_EQ(kWebMFlagKeyframe, block_info.flags)
            << "Text block with timestamp " << block_info.timestamp_in_ms
            << " was not marked as a key frame."
            << " All text blocks must be key frames";
      }

      if (track_number == kAudioTrackNum ||
          track_number == kAlternateAudioTrackNum)
        ASSERT_TRUE(block_info.flags & kWebMFlagKeyframe);

      blocks->push_back(block_info);
    }
  }

  std::unique_ptr<Cluster> GenerateCluster(const std::vector<BlockInfo>& blocks,
                                           bool unknown_size) {
    DCHECK_GT(blocks.size(), 0u);
    ClusterBuilder cb;

    std::vector<uint8_t> data(10);
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (i == 0) cb.SetClusterTimecode(blocks[i].timestamp_in_ms);

      if (blocks[i].duration) {
        if (blocks[i].track_number == kVideoTrackNum ||
            blocks[i].track_number == kAlternateVideoTrackNum) {
          AddVideoBlockGroup(&cb, blocks[i].track_number,
                             blocks[i].timestamp_in_ms, blocks[i].duration,
                             blocks[i].flags);
        } else {
          cb.AddBlockGroup(blocks[i].track_number, blocks[i].timestamp_in_ms,
                           blocks[i].duration, blocks[i].flags,
                           blocks[i].flags & kWebMFlagKeyframe, &data[0],
                           data.size());
        }
      } else {
        cb.AddSimpleBlock(blocks[i].track_number, blocks[i].timestamp_in_ms,
                          blocks[i].flags, &data[0], data.size());
      }
    }

    return unknown_size ? cb.FinishWithUnknownSize() : cb.Finish();
  }

  std::unique_ptr<Cluster> GenerateCluster(
      std::priority_queue<BlockInfo> block_queue, bool unknown_size) {
    std::vector<BlockInfo> blocks(block_queue.size());
    for (size_t i = block_queue.size() - 1; !block_queue.empty(); --i) {
      blocks[i] = block_queue.top();
      block_queue.pop();
    }

    return GenerateCluster(blocks, unknown_size);
  }

  // |block_descriptions| - The block descriptions used to construct the
  // cluster. See the documentation for ParseBlockDescriptions() for details on
  // the string format.
  void AppendSingleStreamCluster(const std::string& source_id, int track_number,
                                 const std::string& block_descriptions) {
    std::vector<BlockInfo> blocks;
    ParseBlockDescriptions(track_number, block_descriptions, &blocks);
    ASSERT_TRUE(AppendCluster(source_id, GenerateCluster(blocks, false)));
  }

  struct MuxedStreamInfo {
    MuxedStreamInfo()
        : track_number(0),
          block_descriptions(""),
          last_blocks_estimated_duration(-1) {}

    MuxedStreamInfo(int track_num, const char* block_desc)
        : track_number(track_num),
          block_descriptions(block_desc),
          last_blocks_estimated_duration(-1) {}

    MuxedStreamInfo(int track_num, const char* block_desc,
                    int last_block_duration_estimate)
        : track_number(track_num),
          block_descriptions(block_desc),
          last_blocks_estimated_duration(last_block_duration_estimate) {}

    int track_number;
    // The block description passed to ParseBlockDescriptions().
    // See the documentation for that method for details on the string format.
    const char* block_descriptions;

    // If -1, no WebMSimpleBlockDurationEstimated MediaLog expectation is added
    // when appending the resulting cluster. Otherwise, an expectation (in ms)
    // is added.
    int last_blocks_estimated_duration;
  };

  void AppendMuxedCluster(const MuxedStreamInfo& msi_1,
                          const MuxedStreamInfo& msi_2) {
    std::vector<MuxedStreamInfo> msi(2);
    msi[0] = msi_1;
    msi[1] = msi_2;
    AppendMuxedCluster(msi);
  }

  void AppendMuxedCluster(const MuxedStreamInfo& msi_1,
                          const MuxedStreamInfo& msi_2,
                          const MuxedStreamInfo& msi_3) {
    std::vector<MuxedStreamInfo> msi(3);
    msi[0] = msi_1;
    msi[1] = msi_2;
    msi[2] = msi_3;
    AppendMuxedCluster(msi);
  }

  std::unique_ptr<Cluster> GenerateMuxedCluster(
      const std::vector<MuxedStreamInfo> msi) {
    std::priority_queue<BlockInfo> block_queue;
    for (size_t i = 0; i < msi.size(); ++i) {
      std::vector<BlockInfo> track_blocks;
      ParseBlockDescriptions(msi[i].track_number, msi[i].block_descriptions,
                             &track_blocks);

      for (size_t j = 0; j < track_blocks.size(); ++j) {
        block_queue.push(track_blocks[j]);
      }

      if (msi[i].last_blocks_estimated_duration != -1) {
        EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(
            msi[i].last_blocks_estimated_duration));
      }
    }
    return GenerateCluster(block_queue, false);
  }

  void AppendMuxedCluster(const std::vector<MuxedStreamInfo> msi) {
    ASSERT_TRUE(AppendCluster(kSourceId, GenerateMuxedCluster(msi)));
  }

  bool AppendData(const std::string& source_id, const uint8_t* data,
                  size_t length) {
    EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());

    return demuxer_->AppendData(
        source_id, data, length, append_window_start_for_next_append_,
        append_window_end_for_next_append_, &timestamp_offset_map_[source_id]);
  }

  bool AppendDataInPieces(const uint8_t* data, size_t length) {
    return AppendDataInPieces(data, length, 7);
  }

  bool AppendDataInPieces(const uint8_t* data, size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size =
          std::min(piece_size, static_cast<size_t>(end - start));
      if (!AppendData(start, append_size)) return false;
      start += append_size;
    }
    return true;
  }

  bool AppendInitSegment(int stream_flags) {
    return AppendInitSegmentWithSourceId(kSourceId, stream_flags);
  }

  bool AppendInitSegmentWithSourceId(const std::string& source_id,
                                     int stream_flags) {
    return AppendInitSegmentWithEncryptedInfo(source_id, stream_flags, false,
                                              false);
  }

  bool AppendInitSegmentWithEncryptedInfo(const std::string& source_id,
                                          int stream_flags,
                                          bool is_audio_encrypted,
                                          bool is_video_encrypted) {
    std::unique_ptr<uint8_t[]> info_tracks;
    int info_tracks_size = 0;
    CreateInitSegment(stream_flags, is_audio_encrypted, is_video_encrypted,
                      &info_tracks, &info_tracks_size);
    return AppendData(source_id, info_tracks.get(), info_tracks_size);
  }

  void AppendGarbage() {
    // Fill up an array with gibberish.
    int garbage_cluster_size = 10;
    std::unique_ptr<uint8_t[]> garbage_cluster(
        new uint8_t[garbage_cluster_size]);
    for (int i = 0; i < garbage_cluster_size; ++i) garbage_cluster[i] = i;
    ASSERT_FALSE(AppendData(garbage_cluster.get(), garbage_cluster_size));
  }

  PipelineStatusCB CreateInitDoneCB(const base::TimeDelta& expected_duration,
                                    PipelineStatus expected_status) {
    if (expected_duration != kNoTimestamp)
      EXPECT_CALL(host_, SetDuration(expected_duration));
    return CreateInitDoneCB(expected_status);
  }

  PipelineStatusCB CreateInitDoneCB(PipelineStatus expected_status) {
    EXPECT_CALL(*this, DemuxerInitialized(expected_status));
    return base::Bind(&ChunkDemuxerTest::DemuxerInitialized,
                      base::Unretained(this));
  }

  enum StreamFlags {
    HAS_AUDIO = 1 << 0,
    HAS_VIDEO = 1 << 1,
    HAS_TEXT = 1 << 2,
    USE_ALTERNATE_AUDIO_TRACK_ID = 1 << 3,
    USE_ALTERNATE_VIDEO_TRACK_ID = 1 << 4,
    USE_ALTERNATE_TEXT_TRACK_ID = 1 << 5,
  };

  bool InitDemuxer(int stream_flags) {
    return InitDemuxerWithEncryptionInfo(stream_flags, false, false);
  }

  void ExpectInitMediaLogs(int stream_flags) {
    if (stream_flags & HAS_VIDEO) {
      EXPECT_MEDIA_LOG(FoundStream("video"));
      EXPECT_MEDIA_LOG(CodecName("video", "vp8"));
    }
    if (stream_flags & HAS_AUDIO) {
      EXPECT_MEDIA_LOG(FoundStream("audio"));
      EXPECT_MEDIA_LOG(CodecName("audio", "vorbis"));
    }
  }

  bool InitDemuxerWithEncryptionInfo(int stream_flags, bool is_audio_encrypted,
                                     bool is_video_encrypted) {
    PipelineStatus expected_status =
        (stream_flags != 0) ? PIPELINE_OK : CHUNK_DEMUXER_ERROR_APPEND_FAILED;

    base::TimeDelta expected_duration = kNoTimestamp;
    if (expected_status == PIPELINE_OK) expected_duration = kDefaultDuration();

    EXPECT_CALL(*this, DemuxerOpened());

    if (is_audio_encrypted || is_video_encrypted) {
      DCHECK(!is_audio_encrypted || stream_flags & HAS_AUDIO);
      DCHECK(!is_video_encrypted || stream_flags & HAS_VIDEO);

      int need_key_count =
          (is_audio_encrypted ? 1 : 0) + (is_video_encrypted ? 1 : 0);
      EXPECT_CALL(*this, OnEncryptedMediaInitData(
                             EmeInitDataType::WEBM,
                             std::vector<uint8_t>(
                                 kEncryptedMediaInitData,
                                 kEncryptedMediaInitData +
                                     arraysize(kEncryptedMediaInitData))))
          .Times(Exactly(need_key_count));
    }

    // Adding expectations prior to CreateInitDoneCB() here because InSequence
    // tests require init segment received before duration set. Also, only
    // expect an init segment received callback if there is actually a track in
    // it.
    if (stream_flags != 0) {
      ExpectInitMediaLogs(stream_flags);
      EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    } else {
      // OnNewConfigs() requires at least one audio, video, or text track.
      EXPECT_MEDIA_LOG(InitSegmentMissesExpectedTrack("vorbis"));
      EXPECT_MEDIA_LOG(InitSegmentMissesExpectedTrack("vp8"));
      EXPECT_MEDIA_LOG(StreamParsingFailed());
    }

    demuxer_->Initialize(
        &host_, CreateInitDoneCB(expected_duration, expected_status), true);

    if (AddId(kSourceId, stream_flags) != ChunkDemuxer::kOk) return false;

    return AppendInitSegmentWithEncryptedInfo(
        kSourceId, stream_flags, is_audio_encrypted, is_video_encrypted);
  }

  bool InitDemuxerAudioAndVideoSourcesText(const std::string& audio_id,
                                           const std::string& video_id,
                                           bool has_text) {
    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK), true);

    if (AddId(audio_id, HAS_AUDIO) != ChunkDemuxer::kOk) return false;
    if (AddId(video_id, HAS_VIDEO) != ChunkDemuxer::kOk) return false;

    int audio_flags = HAS_AUDIO;
    int video_flags = HAS_VIDEO;

    if (has_text) {
      audio_flags |= HAS_TEXT;
      video_flags |= HAS_TEXT;
    }

    // Note: Unlike InitDemuxerWithEncryptionInfo, this method is currently
    // incompatible with InSequence tests. Refactoring of the duration
    // set expectation to not be added during CreateInitDoneCB() could fix this.
    ExpectInitMediaLogs(audio_flags);
    EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    EXPECT_TRUE(AppendInitSegmentWithSourceId(audio_id, audio_flags));

    ExpectInitMediaLogs(video_flags);
    EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    EXPECT_TRUE(AppendInitSegmentWithSourceId(video_id, video_flags));
    return true;
  }

  bool InitDemuxerAudioAndVideoSources(const std::string& audio_id,
                                       const std::string& video_id) {
    return InitDemuxerAudioAndVideoSourcesText(audio_id, video_id, false);
  }

  // Initializes the demuxer with data from 2 files with different
  // decoder configurations. This is used to test the decoder config change
  // logic.
  //
  // bear-320x240.webm VideoDecoderConfig returns 320x240 for its natural_size()
  // bear-640x360.webm VideoDecoderConfig returns 640x360 for its natural_size()
  // The resulting video stream returns data from each file for the following
  // time ranges.
  // bear-320x240.webm : [0-501)       [801-2736)
  // bear-640x360.webm :       [527-760)
  //
  // bear-320x240.webm AudioDecoderConfig returns 3863 for its extra_data size.
  // bear-640x360.webm AudioDecoderConfig returns 3935 for its extra_data size.
  // The resulting audio stream returns data from each file for the following
  // time ranges.
  // bear-320x240.webm : [0-524)       [779-2736)
  // bear-640x360.webm :       [527-759)
  bool InitDemuxerWithConfigChangeData() {
    scoped_refptr<DecoderBuffer> bear1 = ReadTestDataFile("bear-320x240.webm");
    scoped_refptr<DecoderBuffer> bear2 = ReadTestDataFile("bear-640x360.webm");

    EXPECT_CALL(*this, DemuxerOpened());

    // Adding expectation prior to CreateInitDoneCB() here because InSequence
    // tests require init segment received before duration set.
    ExpectInitMediaLogs(HAS_AUDIO | HAS_VIDEO);
    EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    demuxer_->Initialize(
        &host_,
        CreateInitDoneCB(base::TimeDelta::FromMilliseconds(2744), PIPELINE_OK),
        true);

    if (AddId(kSourceId, HAS_AUDIO | HAS_VIDEO) != ChunkDemuxer::kOk)
      return false;

    // Append the whole bear1 file.
    EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2)).Times(7);
    // Expect duration adjustment since actual duration differs slightly from
    // duration in the init segment.
    EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2746)));
    EXPECT_TRUE(AppendData(bear1->data(), bear1->data_size()));
    // Last audio frame has timestamp 2721 and duration 24 (estimated from max
    // seen so far for audio track).
    // Last video frame has timestamp 2703 and duration 33 (from TrackEntry
    // DefaultDuration for video track).
    CheckExpectedRanges("{ [0,2736) }");

    // Append initialization segment for bear2.
    // Note: Offsets here and below are derived from
    // media/test/data/bear-640x360-manifest.js and
    // media/test/data/bear-320x240-manifest.js which were
    // generated from media/test/data/bear-640x360.webm and
    // media/test/data/bear-320x240.webm respectively.
    EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    EXPECT_TRUE(AppendData(bear2->data(), 4340));

    // Append a media segment that goes from [0.527000, 1.014000).
    EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2));
    EXPECT_MEDIA_LOG(GeneratedSplice(20000, 527000));
    EXPECT_TRUE(AppendData(bear2->data() + 55290, 18785));
    CheckExpectedRanges("{ [0,1027) [1201,2736) }");

    // Append initialization segment for bear1 & fill gap with [779-1197)
    // segment.
    EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    EXPECT_TRUE(AppendData(bear1->data(), 4370));
    EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(23));
    EXPECT_MEDIA_LOG(GeneratedSplice(26000, 779000));
    EXPECT_TRUE(AppendData(bear1->data() + 72737, 28183));
    CheckExpectedRanges("{ [0,2736) }");

    MarkEndOfStream(PIPELINE_OK);
    return true;
  }

  void ShutdownDemuxer() {
    if (demuxer_) {
      demuxer_->Shutdown();
      base::RunLoop().RunUntilIdle();
    }
  }

  void AddSimpleBlock(ClusterBuilder* cb, int track_num, int64_t timecode) {
    uint8_t data[] = {0x00};
    cb->AddSimpleBlock(track_num, timecode, 0, data, sizeof(data));
  }

  std::unique_ptr<Cluster> GenerateCluster(int timecode, int block_count) {
    return GenerateCluster(timecode, timecode, block_count);
  }

  void AddVideoBlockGroup(ClusterBuilder* cb, int track_num, int64_t timecode,
                          int duration, int flags) {
    const uint8_t* data =
        (flags & kWebMFlagKeyframe) != 0 ? kVP8Keyframe : kVP8Interframe;
    int size = (flags & kWebMFlagKeyframe) != 0 ? sizeof(kVP8Keyframe)
                                                : sizeof(kVP8Interframe);
    cb->AddBlockGroup(track_num, timecode, duration, flags,
                      flags & kWebMFlagKeyframe, data, size);
  }

  std::unique_ptr<Cluster> GenerateCluster(int first_audio_timecode,
                                           int first_video_timecode,
                                           int block_count) {
    return GenerateCluster(first_audio_timecode, first_video_timecode,
                           block_count, false);
  }
  std::unique_ptr<Cluster> GenerateCluster(int first_audio_timecode,
                                           int first_video_timecode,
                                           int block_count, bool unknown_size) {
    CHECK_GT(block_count, 0);

    std::priority_queue<BlockInfo> block_queue;

    if (block_count == 1) {
      block_queue.push(BlockInfo(kAudioTrackNum, first_audio_timecode,
                                 kWebMFlagKeyframe, kAudioBlockDuration));
      return GenerateCluster(block_queue, unknown_size);
    }

    int audio_timecode = first_audio_timecode;
    int video_timecode = first_video_timecode;

    // Create simple blocks for everything except the last 2 blocks.
    // The first video frame must be a key frame.
    uint8_t video_flag = kWebMFlagKeyframe;
    for (int i = 0; i < block_count - 2; i++) {
      if (audio_timecode <= video_timecode) {
        block_queue.push(
            BlockInfo(kAudioTrackNum, audio_timecode, kWebMFlagKeyframe, 0));
        audio_timecode += kAudioBlockDuration;
        continue;
      }

      block_queue.push(
          BlockInfo(kVideoTrackNum, video_timecode, video_flag, 0));
      video_timecode += kVideoBlockDuration;
      video_flag = 0;
    }

    // Make the last 2 blocks BlockGroups so that they don't get delayed by the
    // block duration calculation logic.
    block_queue.push(BlockInfo(kAudioTrackNum, audio_timecode,
                               kWebMFlagKeyframe, kAudioBlockDuration));
    block_queue.push(BlockInfo(kVideoTrackNum, video_timecode, video_flag,
                               kVideoBlockDuration));

    return GenerateCluster(block_queue, unknown_size);
  }

  std::unique_ptr<Cluster> GenerateSingleStreamCluster(int timecode,
                                                       int end_timecode,
                                                       int track_number,
                                                       int block_duration) {
    CHECK_GT(end_timecode, timecode);

    std::vector<uint8_t> data(kBlockSize);

    ClusterBuilder cb;
    cb.SetClusterTimecode(timecode);

    // Create simple blocks for everything except the last block.
    while (timecode < (end_timecode - block_duration)) {
      cb.AddSimpleBlock(track_number, timecode, kWebMFlagKeyframe, &data[0],
                        data.size());
      timecode += block_duration;
    }

    if (track_number == kVideoTrackNum) {
      AddVideoBlockGroup(&cb, track_number, timecode, block_duration,
                         kWebMFlagKeyframe);
    } else {
      cb.AddBlockGroup(track_number, timecode, block_duration,
                       kWebMFlagKeyframe, static_cast<bool>(kWebMFlagKeyframe),
                       &data[0], data.size());
    }

    return cb.Finish();
  }

  void Read(DemuxerStream::Type type, const DemuxerStream::ReadCB& read_cb) {
    demuxer_->GetStream(type)->Read(read_cb);
    base::RunLoop().RunUntilIdle();
  }

  void ReadAudio(const DemuxerStream::ReadCB& read_cb) {
    Read(DemuxerStream::AUDIO, read_cb);
  }

  void ReadVideo(const DemuxerStream::ReadCB& read_cb) {
    Read(DemuxerStream::VIDEO, read_cb);
  }

  void GenerateExpectedReads(int timecode, int block_count) {
    GenerateExpectedReads(timecode, timecode, block_count);
  }

  void GenerateExpectedReads(int start_audio_timecode, int start_video_timecode,
                             int block_count) {
    CHECK_GT(block_count, 0);

    if (block_count == 1) {
      ExpectRead(DemuxerStream::AUDIO, start_audio_timecode);
      return;
    }

    int audio_timecode = start_audio_timecode;
    int video_timecode = start_video_timecode;

    for (int i = 0; i < block_count; i++) {
      if (audio_timecode <= video_timecode) {
        ExpectRead(DemuxerStream::AUDIO, audio_timecode);
        audio_timecode += kAudioBlockDuration;
        continue;
      }

      ExpectRead(DemuxerStream::VIDEO, video_timecode);
      video_timecode += kVideoBlockDuration;
    }
  }

  void GenerateSingleStreamExpectedReads(int timecode, int block_count,
                                         DemuxerStream::Type type,
                                         int block_duration) {
    CHECK_GT(block_count, 0);
    int stream_timecode = timecode;

    for (int i = 0; i < block_count; i++) {
      ExpectRead(type, stream_timecode);
      stream_timecode += block_duration;
    }
  }

  void GenerateAudioStreamExpectedReads(int timecode, int block_count) {
    GenerateSingleStreamExpectedReads(
        timecode, block_count, DemuxerStream::AUDIO, kAudioBlockDuration);
  }

  void GenerateVideoStreamExpectedReads(int timecode, int block_count) {
    GenerateSingleStreamExpectedReads(
        timecode, block_count, DemuxerStream::VIDEO, kVideoBlockDuration);
  }

  std::unique_ptr<Cluster> GenerateEmptyCluster(int timecode) {
    ClusterBuilder cb;
    cb.SetClusterTimecode(timecode);
    return cb.Finish();
  }

  void CheckExpectedRangesForMediaSource(const std::string& expected) {
    CheckExpectedRanges(demuxer_->GetBufferedRanges(), expected);
  }

  void CheckExpectedRanges(const std::string& expected) {
    CheckExpectedRanges(kSourceId, expected);
    CheckExpectedRangesForMediaSource(expected);
  }

  void CheckExpectedRanges(const std::string& id, const std::string& expected) {
    CheckExpectedRanges(demuxer_->GetBufferedRanges(id), expected);
  }

  void CheckExpectedRanges(DemuxerStream::Type type,
                           const std::string& expected) {
    ChunkDemuxerStream* stream =
        static_cast<ChunkDemuxerStream*>(demuxer_->GetStream(type));
    CheckExpectedRanges(stream->GetBufferedRanges(kDefaultDuration()),
                        expected);
  }

  void CheckExpectedRanges(const Ranges<base::TimeDelta>& r,
                           const std::string& expected) {
    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      ss << "[" << r.start(i).InMilliseconds() << ","
         << r.end(i).InMilliseconds() << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  MOCK_METHOD2(ReadDone, void(DemuxerStream::Status status,
                              const scoped_refptr<DecoderBuffer>&));

  void StoreStatusAndBuffer(DemuxerStream::Status* status_out,
                            scoped_refptr<DecoderBuffer>* buffer_out,
                            DemuxerStream::Status status,
                            const scoped_refptr<DecoderBuffer>& buffer) {
    *status_out = status;
    *buffer_out = buffer;
  }

  void ReadUntilNotOkOrEndOfStream(DemuxerStream::Type type,
                                   DemuxerStream::Status* status,
                                   base::TimeDelta* last_timestamp) {
    DemuxerStream* stream = demuxer_->GetStream(type);
    scoped_refptr<DecoderBuffer> buffer;

    *last_timestamp = kNoTimestamp;
    do {
      stream->Read(base::Bind(&ChunkDemuxerTest::StoreStatusAndBuffer,
                              base::Unretained(this), status, &buffer));
      base::RunLoop().RunUntilIdle();
      if (*status == DemuxerStream::kOk && !buffer->end_of_stream())
        *last_timestamp = buffer->timestamp();
    } while (*status == DemuxerStream::kOk && !buffer->end_of_stream());
  }

  void ExpectEndOfStream(DemuxerStream::Type type) {
    EXPECT_CALL(*this, ReadDone(DemuxerStream::kOk, IsEndOfStream()));
    demuxer_->GetStream(type)
        ->Read(base::Bind(&ChunkDemuxerTest::ReadDone, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectRead(DemuxerStream::Type type, int64_t timestamp_in_ms) {
    EXPECT_CALL(*this,
                ReadDone(DemuxerStream::kOk, HasTimestamp(timestamp_in_ms)));
    demuxer_->GetStream(type)
        ->Read(base::Bind(&ChunkDemuxerTest::ReadDone, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectConfigChanged(DemuxerStream::Type type) {
    EXPECT_CALL(*this, ReadDone(DemuxerStream::kConfigChanged, _));
    demuxer_->GetStream(type)
        ->Read(base::Bind(&ChunkDemuxerTest::ReadDone, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void CheckExpectedBuffers(DemuxerStream* stream,
                            const std::string& expected) {
    std::vector<std::string> timestamps = base::SplitString(
        expected, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::stringstream ss;
    for (size_t i = 0; i < timestamps.size(); ++i) {
      // Initialize status to kAborted since it's possible for Read() to return
      // without calling StoreStatusAndBuffer() if it doesn't have any buffers
      // left to return.
      DemuxerStream::Status status = DemuxerStream::kAborted;
      scoped_refptr<DecoderBuffer> buffer;
      stream->Read(base::Bind(&ChunkDemuxerTest::StoreStatusAndBuffer,
                              base::Unretained(this), &status, &buffer));
      base::RunLoop().RunUntilIdle();
      if (status != DemuxerStream::kOk || buffer->end_of_stream()) break;

      if (i > 0) ss << " ";
      ss << buffer->timestamp().InMilliseconds();

      if (buffer->is_key_frame()) ss << "K";

      // Handle preroll buffers.
      if (base::EndsWith(timestamps[i], "P", base::CompareCase::SENSITIVE)) {
        ASSERT_EQ(kInfiniteDuration, buffer->discard_padding().first);
        ASSERT_EQ(base::TimeDelta(), buffer->discard_padding().second);
        ss << "P";
      }
    }
    EXPECT_EQ(expected, ss.str());
  }

  MOCK_METHOD1(Checkpoint, void(int id));

  struct BufferTimestamps {
    int video_time_ms;
    int audio_time_ms;
  };
  static const int kSkip = -1;

  // Test parsing a WebM file.
  // |filename| - The name of the file in media/test/data to parse.
  // |timestamps| - The expected timestamps on the parsed buffers.
  //    a timestamp of kSkip indicates that a Read() call for that stream
  //    shouldn't be made on that iteration of the loop. If both streams have
  //    a kSkip then the loop will terminate.
  bool ParseWebMFile(const std::string& filename,
                     const BufferTimestamps* timestamps,
                     const base::TimeDelta& duration) {
    return ParseWebMFile(filename, timestamps, duration, HAS_AUDIO | HAS_VIDEO);
  }

  bool ParseWebMFile(const std::string& filename,
                     const BufferTimestamps* timestamps,
                     const base::TimeDelta& duration, int stream_flags) {
    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(&host_, CreateInitDoneCB(duration, PIPELINE_OK), true);

    if (AddId(kSourceId, stream_flags) != ChunkDemuxer::kOk) return false;

    // Read a WebM file into memory and send the data to the demuxer.
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_CALL(*this, InitSegmentReceivedMock(_));
    EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));

    // Verify that the timestamps on the first few packets match what we
    // expect.
    for (size_t i = 0; (timestamps[i].audio_time_ms != kSkip ||
                        timestamps[i].video_time_ms != kSkip);
         i++) {
      bool audio_read_done = false;
      bool video_read_done = false;

      if (timestamps[i].audio_time_ms != kSkip) {
        ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(
                                              timestamps[i].audio_time_ms),
                             &audio_read_done));
        EXPECT_TRUE(audio_read_done);
      }

      if (timestamps[i].video_time_ms != kSkip) {
        ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(
                                              timestamps[i].video_time_ms),
                             &video_read_done));
        EXPECT_TRUE(video_read_done);
      }
    }

    return true;
  }

  MOCK_METHOD0(DemuxerOpened, void());
  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data));

  MOCK_METHOD1(InitSegmentReceivedMock, void(std::unique_ptr<MediaTracks>&));

  void Seek(base::TimeDelta seek_time) {
    demuxer_->StartWaitingForSeek(seek_time);
    demuxer_->Seek(seek_time, NewExpectedStatusCB(PIPELINE_OK));
    base::RunLoop().RunUntilIdle();
  }

  void MarkEndOfStream(PipelineStatus status) {
    demuxer_->MarkEndOfStream(status);
    base::RunLoop().RunUntilIdle();
  }

  bool SetTimestampOffset(const std::string& id,
                          base::TimeDelta timestamp_offset) {
    if (demuxer_->IsParsingMediaSegment(id)) return false;

    timestamp_offset_map_[id] = timestamp_offset;
    return true;
  }

  base::MessageLoop message_loop_;
  MockDemuxerHost host_;

  scoped_refptr<StrictMock<MockMediaLog>> media_log_;

  std::unique_ptr<ChunkDemuxer> demuxer_;
  Demuxer::MediaTracksUpdatedCB init_segment_received_cb_;

  base::TimeDelta append_window_start_for_next_append_;
  base::TimeDelta append_window_end_for_next_append_;

  // Map of source id to timestamp offset to use for the next AppendData()
  // operation for that source id.
  std::map<std::string, base::TimeDelta> timestamp_offset_map_;

 public:
  void InitSegmentReceived(std::unique_ptr<MediaTracks> tracks) {
    DCHECK(tracks.get());
    DCHECK_GT(tracks->tracks().size(), 0u);

    // Verify that track ids are unique.
    std::set<MediaTrack::Id> track_ids;
    for (const auto& track : tracks->tracks()) {
      EXPECT_EQ(track_ids.end(), track_ids.find(track->id()));
      track_ids.insert(track->id());
    }

    InitSegmentReceivedMock(tracks);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerTest);
};

TEST_F(ChunkDemuxerTest, Init) {
  InSequence s;

  // Test no streams, audio-only, video-only, and audio & video scenarios.
  // Audio and video streams can be encrypted or not encrypted.
  for (int i = 0; i < 16; i++) {
    bool has_audio = (i & 0x1) != 0;
    bool has_video = (i & 0x2) != 0;
    bool is_audio_encrypted = (i & 0x4) != 0;
    bool is_video_encrypted = (i & 0x8) != 0;

    // No test on invalid combination.
    if ((!has_audio && is_audio_encrypted) ||
        (!has_video && is_video_encrypted)) {
      continue;
    }

    CreateNewDemuxer();

    int stream_flags = 0;
    if (has_audio) stream_flags |= HAS_AUDIO;

    if (has_video) stream_flags |= HAS_VIDEO;

    if (has_audio || has_video) {
      ASSERT_TRUE(InitDemuxerWithEncryptionInfo(
          stream_flags, is_audio_encrypted, is_video_encrypted));
    } else {
      ASSERT_FALSE(InitDemuxerWithEncryptionInfo(
          stream_flags, is_audio_encrypted, is_video_encrypted));
    }

    DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
    if (has_audio) {
      ASSERT_TRUE(audio_stream);

      const AudioDecoderConfig& config = audio_stream->audio_decoder_config();
      EXPECT_EQ(kCodecVorbis, config.codec());
      EXPECT_EQ(32, config.bits_per_channel());
      EXPECT_EQ(CHANNEL_LAYOUT_STEREO, config.channel_layout());
      EXPECT_EQ(44100, config.samples_per_second());
      EXPECT_GT(config.extra_data().size(), 0u);
      EXPECT_EQ(kSampleFormatPlanarF32, config.sample_format());
      EXPECT_EQ(is_audio_encrypted,
                audio_stream->audio_decoder_config().is_encrypted());
      EXPECT_TRUE(static_cast<ChunkDemuxerStream*>(audio_stream)
                      ->supports_partial_append_window_trimming());
    } else {
      EXPECT_FALSE(audio_stream);
    }

    DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
    if (has_video) {
      EXPECT_TRUE(video_stream);
      EXPECT_EQ(is_video_encrypted,
                video_stream->video_decoder_config().is_encrypted());
      EXPECT_FALSE(static_cast<ChunkDemuxerStream*>(video_stream)
                       ->supports_partial_append_window_trimming());
    } else {
      EXPECT_FALSE(video_stream);
    }

    ShutdownDemuxer();
    demuxer_.reset();
  }
}

// TODO(acolwell): Fold this test into Init tests since the tests are
// almost identical.
TEST_F(ChunkDemuxerTest, InitText) {
  // Test with 1 video stream and 1 text streams, and 0 or 1 audio streams.
  // No encryption cases handled here.
  bool has_video = true;
  bool is_audio_encrypted = false;
  bool is_video_encrypted = false;
  for (int i = 0; i < 2; i++) {
    bool has_audio = (i & 0x1) != 0;

    CreateNewDemuxer();

    DemuxerStream* text_stream = NULL;
    TextTrackConfig text_config;
    EXPECT_CALL(host_, AddTextStream(_, _))
        .WillOnce(DoAll(SaveArg<0>(&text_stream), SaveArg<1>(&text_config)));

    int stream_flags = HAS_TEXT;
    if (has_audio) stream_flags |= HAS_AUDIO;

    if (has_video) stream_flags |= HAS_VIDEO;

    ASSERT_TRUE(InitDemuxerWithEncryptionInfo(stream_flags, is_audio_encrypted,
                                              is_video_encrypted));
    ASSERT_TRUE(text_stream);
    EXPECT_EQ(DemuxerStream::TEXT, text_stream->type());
    EXPECT_EQ(kTextSubtitles, text_config.kind());
    EXPECT_FALSE(static_cast<ChunkDemuxerStream*>(text_stream)
                     ->supports_partial_append_window_trimming());

    DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
    if (has_audio) {
      ASSERT_TRUE(audio_stream);

      const AudioDecoderConfig& config = audio_stream->audio_decoder_config();
      EXPECT_EQ(kCodecVorbis, config.codec());
      EXPECT_EQ(32, config.bits_per_channel());
      EXPECT_EQ(CHANNEL_LAYOUT_STEREO, config.channel_layout());
      EXPECT_EQ(44100, config.samples_per_second());
      EXPECT_GT(config.extra_data().size(), 0u);
      EXPECT_EQ(kSampleFormatPlanarF32, config.sample_format());
      EXPECT_EQ(is_audio_encrypted,
                audio_stream->audio_decoder_config().is_encrypted());
      EXPECT_TRUE(static_cast<ChunkDemuxerStream*>(audio_stream)
                      ->supports_partial_append_window_trimming());
    } else {
      EXPECT_FALSE(audio_stream);
    }

    DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
    if (has_video) {
      EXPECT_TRUE(video_stream);
      EXPECT_EQ(is_video_encrypted,
                video_stream->video_decoder_config().is_encrypted());
      EXPECT_FALSE(static_cast<ChunkDemuxerStream*>(video_stream)
                       ->supports_partial_append_window_trimming());
    } else {
      EXPECT_FALSE(video_stream);
    }

    ShutdownDemuxer();
    demuxer_.reset();
  }
}

TEST_F(ChunkDemuxerTest, SingleTextTrackIdChange) {
  // Test with 1 video stream, 1 audio, and 1 text stream. Send a second init
  // segment in which the text track ID changes. Verify appended buffers before
  // and after the second init segment map to the same underlying track buffers.
  CreateNewDemuxer();
  DemuxerStream* text_stream = NULL;
  TextTrackConfig text_config;
  EXPECT_CALL(host_, AddTextStream(_, _))
      .WillOnce(DoAll(SaveArg<0>(&text_stream), SaveArg<1>(&text_config)));
  ASSERT_TRUE(InitDemuxerWithEncryptionInfo(HAS_TEXT | HAS_AUDIO | HAS_VIDEO,
                                            false, false));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(audio_stream);
  ASSERT_TRUE(video_stream);
  ASSERT_TRUE(text_stream);

  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0K 23K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "0K 30", 30),
                     MuxedStreamInfo(kTextTrackNum, "10K"));
  CheckExpectedRanges("{ [0,46) }");

  std::unique_ptr<uint8_t[]> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(
      HAS_TEXT | HAS_AUDIO | HAS_VIDEO | USE_ALTERNATE_TEXT_TRACK_ID, false,
      false, &info_tracks, &info_tracks_size);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(demuxer_->AppendData(
      kSourceId, info_tracks.get(), info_tracks_size,
      append_window_start_for_next_append_, append_window_end_for_next_append_,
      &timestamp_offset_map_[kSourceId]));

  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum, "46K 69K", 23),
      MuxedStreamInfo(kVideoTrackNum, "60K",
                      WebMClusterParser::kDefaultVideoBufferDurationInMs),
      MuxedStreamInfo(kAlternateTextTrackNum, "45K"));

  CheckExpectedRanges("{ [0,92) }");
  CheckExpectedBuffers(audio_stream, "0K 23K 46K 69K");
  CheckExpectedBuffers(video_stream, "0K 30 60K");
  CheckExpectedBuffers(text_stream, "10K 45K");

  ShutdownDemuxer();
}

TEST_F(ChunkDemuxerTest, AudioVideoTrackIdsChange) {
  // Test with 1 audio and 1 video stream. Send a second init segment in which
  // the audio and video track IDs change. Verify that appended buffers before
  // and after the second init segment map to the same underlying track buffers.
  CreateNewDemuxer();
  ASSERT_TRUE(
      InitDemuxerWithEncryptionInfo(HAS_AUDIO | HAS_VIDEO, false, false));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(audio_stream);
  ASSERT_TRUE(video_stream);

  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0K 23K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "0K 30", 30));
  CheckExpectedRanges("{ [0,46) }");

  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendInitSegment(HAS_AUDIO | HAS_VIDEO |
                                USE_ALTERNATE_AUDIO_TRACK_ID |
                                USE_ALTERNATE_VIDEO_TRACK_ID));
  AppendMuxedCluster(MuxedStreamInfo(kAlternateAudioTrackNum, "46K 69K", 63),
                     MuxedStreamInfo(kAlternateVideoTrackNum, "60K", 23));
  CheckExpectedRanges("{ [0,92) }");
  CheckExpectedBuffers(audio_stream, "0K 23K 46K 69K");
  CheckExpectedBuffers(video_stream, "0K 30 60K");

  ShutdownDemuxer();
}

TEST_F(ChunkDemuxerTest, InitSegmentSetsNeedRandomAccessPointFlag) {
  // Tests that non-key-frames following an init segment are allowed
  // and dropped, as expected if the initialization segment received
  // algorithm correctly sets the needs random access point flag to true for all
  // track buffers. Note that the first initialization segment is insufficient
  // to fully test this since needs random access point flag initializes to
  // true.
  CreateNewDemuxer();
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _)).WillOnce(SaveArg<0>(&text_stream));
  ASSERT_TRUE(InitDemuxerWithEncryptionInfo(HAS_TEXT | HAS_AUDIO | HAS_VIDEO,
                                            false, false));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(audio_stream && video_stream && text_stream);

  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum, "23K",
                      WebMClusterParser::kDefaultAudioBufferDurationInMs),
      MuxedStreamInfo(kVideoTrackNum, "0 30K", 30),
      MuxedStreamInfo(kTextTrackNum, "25K 40K"));
  CheckExpectedRanges("{ [23,46) }");

  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendInitSegment(HAS_TEXT | HAS_AUDIO | HAS_VIDEO));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "46K 69K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "60 90K", 30),
                     MuxedStreamInfo(kTextTrackNum, "80K 90K"));
  CheckExpectedRanges("{ [23,92) }");

  CheckExpectedBuffers(audio_stream, "23K 46K 69K");
  CheckExpectedBuffers(video_stream, "30K 90K");
  CheckExpectedBuffers(text_stream, "25K 40K 80K 90K");
}

TEST_F(ChunkDemuxerTest, Shutdown_BeforeAllInitSegmentsAppended) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_, base::Bind(&ChunkDemuxerTest::DemuxerInitialized,
                                          base::Unretained(this)),
                       true);

  EXPECT_EQ(AddId("audio", HAS_AUDIO), ChunkDemuxer::kOk);
  EXPECT_EQ(AddId("video", HAS_VIDEO), ChunkDemuxer::kOk);

  ExpectInitMediaLogs(HAS_AUDIO);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendInitSegmentWithSourceId("audio", HAS_AUDIO));

  ShutdownDemuxer();
}

TEST_F(ChunkDemuxerTest, Shutdown_BeforeAllInitSegmentsAppendedText) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_, base::Bind(&ChunkDemuxerTest::DemuxerInitialized,
                                          base::Unretained(this)),
                       true);

  EXPECT_EQ(AddId("audio", HAS_AUDIO), ChunkDemuxer::kOk);
  EXPECT_EQ(AddId("video_and_text", HAS_VIDEO), ChunkDemuxer::kOk);

  EXPECT_CALL(host_, AddTextStream(_, _)).Times(Exactly(1));

  ExpectInitMediaLogs(HAS_VIDEO);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(
      AppendInitSegmentWithSourceId("video_and_text", HAS_VIDEO | HAS_TEXT));

  ShutdownDemuxer();
}

// Verifies that all streams waiting for data receive an end of stream
// buffer when Shutdown() is called.
TEST_F(ChunkDemuxerTest, Shutdown_EndOfStreamWhileWaitingForData) {
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _)).WillOnce(SaveArg<0>(&text_stream));
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO | HAS_TEXT));

  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done = false;
  bool video_read_done = false;
  bool text_read_done = false;
  audio_stream->Read(base::Bind(&OnReadDone_EOSExpected, &audio_read_done));
  video_stream->Read(base::Bind(&OnReadDone_EOSExpected, &video_read_done));
  text_stream->Read(base::Bind(&OnReadDone_EOSExpected, &text_read_done));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);
  EXPECT_FALSE(text_read_done);

  ShutdownDemuxer();

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
  EXPECT_TRUE(text_read_done);
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, AppendDataAfterSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  InSequence s;

  EXPECT_CALL(*this, Checkpoint(1));

  Seek(base::TimeDelta::FromMilliseconds(46));

  EXPECT_CALL(*this, Checkpoint(2));

  Checkpoint(1);

  ASSERT_TRUE(AppendCluster(kDefaultSecondCluster()));

  base::RunLoop().RunUntilIdle();

  Checkpoint(2);
}

// Test that parsing errors are handled for clusters appended after init.
TEST_F(ChunkDemuxerTest, ErrorWhileParsingClusterAfterInit) {
  InSequence s;
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  EXPECT_MEDIA_LOG(StreamParsingFailed());
  EXPECT_CALL(host_, OnDemuxerError(CHUNK_DEMUXER_ERROR_APPEND_FAILED));
  AppendGarbage();
}

// Test the case where a Seek() is requested while the parser
// is in the middle of cluster. This is to verify that the parser
// does not reset itself on a seek.
TEST_F(ChunkDemuxerTest, SeekWhileParsingCluster) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  InSequence s;

  std::unique_ptr<Cluster> cluster_a(GenerateCluster(0, 6));

  // Split the cluster into two appends at an arbitrary point near the end.
  int first_append_size = cluster_a->size() - 11;
  int second_append_size = cluster_a->size() - first_append_size;

  // Append the first part of the cluster.
  ASSERT_TRUE(AppendData(cluster_a->data(), first_append_size));

  ExpectRead(DemuxerStream::AUDIO, 0);
  ExpectRead(DemuxerStream::VIDEO, 0);
  ExpectRead(DemuxerStream::AUDIO, kAudioBlockDuration);

  Seek(base::TimeDelta::FromSeconds(5));

  // Append the rest of the cluster.
  ASSERT_TRUE(
      AppendData(cluster_a->data() + first_append_size, second_append_size));

  // Append the new cluster and verify that only the blocks
  // in the new cluster are returned.
  ASSERT_TRUE(AppendCluster(GenerateCluster(5000, 6)));
  GenerateExpectedReads(5000, 6);
}

// Test the case where AppendData() is called before Init().
TEST_F(ChunkDemuxerTest, AppendDataBeforeInit) {
  std::unique_ptr<uint8_t[]> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(HAS_AUDIO | HAS_VIDEO, false, false, &info_tracks,
                    &info_tracks_size);
  ASSERT_FALSE(demuxer_->AppendData(
      kSourceId, info_tracks.get(), info_tracks_size,
      append_window_start_for_next_append_, append_window_end_for_next_append_,
      &timestamp_offset_map_[kSourceId]));
}

// Make sure Read() callbacks are dispatched with the proper data.
TEST_F(ChunkDemuxerTest, Read) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &video_read_done));

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, OutOfOrderClusters) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));
  EXPECT_MEDIA_LOG(GeneratedSplice(13000, 10000));
  ASSERT_TRUE(AppendCluster(GenerateCluster(10, 4)));

  // Make sure that AppendCluster() does not fail with a cluster that has
  // overlaps with the previously appended cluster.
  EXPECT_MEDIA_LOG(SkippingSpliceAlreadySpliced(0));
  ASSERT_TRUE(AppendCluster(GenerateCluster(5, 4)));

  // Verify that AppendData() can still accept more data.
  std::unique_ptr<Cluster> cluster_c(GenerateCluster(45, 2));
  EXPECT_MEDIA_LOG(GeneratedSplice(6000, 45000));
  ASSERT_TRUE(demuxer_->AppendData(
      kSourceId, cluster_c->data(), cluster_c->size(),
      append_window_start_for_next_append_, append_window_end_for_next_append_,
      &timestamp_offset_map_[kSourceId]));
}

TEST_F(ChunkDemuxerTest, NonMonotonicButAboveClusterTimecode) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  ClusterBuilder cb;

  // Test the case where block timecodes are not monotonically
  // increasing but stay above the cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 7);
  AddSimpleBlock(&cb, kVideoTrackNum, 15);

  EXPECT_MEDIA_LOG(WebMOutOfOrderTimecode());
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  EXPECT_CALL(host_, OnDemuxerError(CHUNK_DEMUXER_ERROR_APPEND_FAILED));
  ASSERT_FALSE(AppendCluster(cb.Finish()));

  // Verify that AppendData() ignores data after the error.
  std::unique_ptr<Cluster> cluster_b(GenerateCluster(20, 2));
  ASSERT_FALSE(demuxer_->AppendData(
      kSourceId, cluster_b->data(), cluster_b->size(),
      append_window_start_for_next_append_, append_window_end_for_next_append_,
      &timestamp_offset_map_[kSourceId]));
}

TEST_F(ChunkDemuxerTest, BackwardsAndBeforeClusterTimecode) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  ClusterBuilder cb;

  // Test timecodes going backwards and including values less than the cluster
  // timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 3);
  AddSimpleBlock(&cb, kVideoTrackNum, 3);

  EXPECT_MEDIA_LOG(WebMNegativeTimecodeOffset("-2"));
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  EXPECT_CALL(host_, OnDemuxerError(CHUNK_DEMUXER_ERROR_APPEND_FAILED));
  ASSERT_FALSE(AppendCluster(cb.Finish()));

  // Verify that AppendData() ignores data after the error.
  std::unique_ptr<Cluster> cluster_b(GenerateCluster(6, 2));
  ASSERT_FALSE(demuxer_->AppendData(
      kSourceId, cluster_b->data(), cluster_b->size(),
      append_window_start_for_next_append_, append_window_end_for_next_append_,
      &timestamp_offset_map_[kSourceId]));
}

TEST_F(ChunkDemuxerTest, PerStreamMonotonicallyIncreasingTimestamps) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  ClusterBuilder cb;

  // Test monotonic increasing timestamps on a per stream
  // basis.
  cb.SetClusterTimecode(4);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 4);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);

  EXPECT_MEDIA_LOG(WebMOutOfOrderTimecode());
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  EXPECT_CALL(host_, OnDemuxerError(CHUNK_DEMUXER_ERROR_APPEND_FAILED));
  ASSERT_FALSE(AppendCluster(cb.Finish()));
}

// Test the case where a cluster is passed to AppendCluster() before
// INFO & TRACKS data.
TEST_F(ChunkDemuxerTest, ClusterBeforeInitSegment) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(CHUNK_DEMUXER_ERROR_APPEND_FAILED), true);

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  EXPECT_MEDIA_LOG(WebMClusterBeforeFirstInfo());
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  ASSERT_FALSE(AppendCluster(GenerateCluster(0, 1)));
}

// Test cases where we get an MarkEndOfStream() call during initialization.
TEST_F(ChunkDemuxerTest, EOSDuringInit) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_,
                       NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN), true);
  MarkEndOfStream(PIPELINE_OK);
}

TEST_F(ChunkDemuxerTest, EndOfStreamWithNoAppend) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_,
                       NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN), true);

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  CheckExpectedRanges("{ }");
  MarkEndOfStream(PIPELINE_OK);
  ShutdownDemuxer();
  CheckExpectedRanges("{ }");
  demuxer_->RemoveId(kSourceId);
  demuxer_.reset();
}

TEST_F(ChunkDemuxerTest, EndOfStreamWithNoMediaAppend) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  CheckExpectedRanges("{ }");
  MarkEndOfStream(PIPELINE_OK);
  CheckExpectedRanges("{ }");
}

TEST_F(ChunkDemuxerTest, DecodeErrorEndOfStream) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));
  CheckExpectedRanges(kDefaultFirstClusterRange);

  EXPECT_CALL(host_,
              OnDemuxerError(CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR));
  MarkEndOfStream(CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR);
  CheckExpectedRanges(kDefaultFirstClusterRange);
}

TEST_F(ChunkDemuxerTest, NetworkErrorEndOfStream) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));
  CheckExpectedRanges(kDefaultFirstClusterRange);

  EXPECT_CALL(host_,
              OnDemuxerError(CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR));
  MarkEndOfStream(CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR);
}

// Helper class to reduce duplicate code when testing end of stream
// Read() behavior.
class EndOfStreamHelper {
 public:
  explicit EndOfStreamHelper(Demuxer* demuxer)
      : demuxer_(demuxer), audio_read_done_(false), video_read_done_(false) {}

  // Request a read on the audio and video streams.
  void RequestReads() {
    EXPECT_FALSE(audio_read_done_);
    EXPECT_FALSE(video_read_done_);

    DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
    DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

    audio->Read(base::Bind(&OnEndOfStreamReadDone, &audio_read_done_));
    video->Read(base::Bind(&OnEndOfStreamReadDone, &video_read_done_));
    base::RunLoop().RunUntilIdle();
  }

  // Check to see if |audio_read_done_| and |video_read_done_| variables
  // match |expected|.
  void CheckIfReadDonesWereCalled(bool expected) {
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected, audio_read_done_);
    EXPECT_EQ(expected, video_read_done_);
  }

 private:
  static void OnEndOfStreamReadDone(
      bool* called, DemuxerStream::Status status,
      const scoped_refptr<DecoderBuffer>& buffer) {
    EXPECT_EQ(status, DemuxerStream::kOk);
    EXPECT_TRUE(buffer->end_of_stream());
    *called = true;
  }

  Demuxer* demuxer_;
  bool audio_read_done_;
  bool video_read_done_;

  DISALLOW_COPY_AND_ASSIGN(EndOfStreamHelper);
};

// Make sure that all pending reads that we don't have media data for get an
// "end of stream" buffer when MarkEndOfStream() is called.
TEST_F(ChunkDemuxerTest, EndOfStreamWithPendingReads) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 2)));

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_.get());
  EndOfStreamHelper end_of_stream_helper_2(demuxer_.get());

  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done_1));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &video_read_done_1));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);

  end_of_stream_helper_1.RequestReads();

  EXPECT_CALL(
      host_,
      SetDuration(base::TimeDelta::FromMilliseconds(kVideoBlockDuration)));
  MarkEndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);

  end_of_stream_helper_2.RequestReads();
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);
}

// Make sure that all Read() calls after we get an MarkEndOfStream()
// call return an "end of stream" buffer.
TEST_F(ChunkDemuxerTest, ReadsAfterEndOfStream) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 2)));

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_.get());
  EndOfStreamHelper end_of_stream_helper_2(demuxer_.get());
  EndOfStreamHelper end_of_stream_helper_3(demuxer_.get());

  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done_1));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &video_read_done_1));

  end_of_stream_helper_1.RequestReads();

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);
  end_of_stream_helper_1.CheckIfReadDonesWereCalled(false);

  EXPECT_CALL(
      host_,
      SetDuration(base::TimeDelta::FromMilliseconds(kVideoBlockDuration)));
  MarkEndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);

  // Request a few more reads and make sure we immediately get
  // end of stream buffers.
  end_of_stream_helper_2.RequestReads();
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);

  end_of_stream_helper_3.RequestReads();
  end_of_stream_helper_3.CheckIfReadDonesWereCalled(true);
}

TEST_F(ChunkDemuxerTest, EndOfStreamDuringCanceledSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(0, 10));
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(138)));
  MarkEndOfStream(PIPELINE_OK);

  // Start the first seek.
  Seek(base::TimeDelta::FromMilliseconds(20));

  // Simulate another seek being requested before the first
  // seek has finished prerolling.
  base::TimeDelta seek_time2 = base::TimeDelta::FromMilliseconds(30);
  demuxer_->CancelPendingSeek(seek_time2);

  // Finish second seek.
  Seek(seek_time2);

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  // Make sure audio can reach end of stream.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);

  // Make sure video can reach end of stream.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);
}

// Verify buffered range change behavior for audio/video/text tracks.
TEST_F(ChunkDemuxerTest, EndOfStreamRangeChanges) {
  DemuxerStream* text_stream = NULL;

  EXPECT_CALL(host_, AddTextStream(_, _)).WillOnce(SaveArg<0>(&text_stream));
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO | HAS_TEXT));

  AppendMuxedCluster(MuxedStreamInfo(kVideoTrackNum, "0K 33", 33),
                     MuxedStreamInfo(kAudioTrackNum, "0K 23K", 23));

  // Check expected ranges and verify that an empty text track does not
  // affect the expected ranges.
  CheckExpectedRanges("{ [0,46) }");

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(66)));
  MarkEndOfStream(PIPELINE_OK);

  // Check expected ranges and verify that an empty text track does not
  // affect the expected ranges.
  CheckExpectedRanges("{ [0,66) }");

  // Unmark end of stream state and verify that the ranges return to
  // their pre-"end of stream" values.
  demuxer_->UnmarkEndOfStream();
  CheckExpectedRanges("{ [0,46) }");

  // Add text track data and verify that the buffered ranges don't change
  // since the intersection of all the tracks doesn't change.
  EXPECT_MEDIA_LOG(SkippingSpliceAtOrBefore(0, 0));
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(200)));
  AppendMuxedCluster(MuxedStreamInfo(kVideoTrackNum, "0K 33", 33),
                     MuxedStreamInfo(kAudioTrackNum, "0K 23K", 23),
                     MuxedStreamInfo(kTextTrackNum, "0K 100K"));
  CheckExpectedRanges("{ [0,46) }");

  // Mark end of stream and verify that text track data is reflected in
  // the new range.
  MarkEndOfStream(PIPELINE_OK);
  CheckExpectedRanges("{ [0,200) }");
}

// Make sure AppendData() will accept elements that span multiple calls.
TEST_F(ChunkDemuxerTest, AppendingInPieces) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_,
                       CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK), true);

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  std::unique_ptr<uint8_t[]> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(HAS_AUDIO | HAS_VIDEO, false, false, &info_tracks,
                    &info_tracks_size);

  std::unique_ptr<Cluster> cluster_a(kDefaultFirstCluster());
  std::unique_ptr<Cluster> cluster_b(kDefaultSecondCluster());

  size_t buffer_size = info_tracks_size + cluster_a->size() + cluster_b->size();
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  uint8_t* dst = buffer.get();
  memcpy(dst, info_tracks.get(), info_tracks_size);
  dst += info_tracks_size;

  memcpy(dst, cluster_a->data(), cluster_a->size());
  dst += cluster_a->size();

  memcpy(dst, cluster_b->data(), cluster_b->size());
  dst += cluster_b->size();

  ExpectInitMediaLogs(HAS_AUDIO | HAS_VIDEO);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendDataInPieces(buffer.get(), buffer_size));

  GenerateExpectedReads(0, 9);
}

TEST_F(ChunkDemuxerTest, WebMFile_AudioAndVideo) {
  struct BufferTimestamps buffer_timestamps[] = {
      {0, 0}, {33, 3}, {67, 6}, {100, 9}, {133, 12}, {kSkip, kSkip},
  };

  ExpectInitMediaLogs(HAS_AUDIO | HAS_VIDEO);
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2)).Times(7);

  // Expect duration adjustment since actual duration differs slightly from
  // duration in the init segment.
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2746)));

  ASSERT_TRUE(ParseWebMFile("bear-320x240.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2744)));
  EXPECT_EQ(212949, demuxer_->GetMemoryUsage());
}

TEST_F(ChunkDemuxerTest, WebMFile_LiveAudioAndVideo) {
  struct BufferTimestamps buffer_timestamps[] = {
      {0, 0}, {33, 3}, {67, 6}, {100, 9}, {133, 12}, {kSkip, kSkip},
  };

  ExpectInitMediaLogs(HAS_AUDIO | HAS_VIDEO);
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2)).Times(7);
  ASSERT_TRUE(ParseWebMFile("bear-320x240-live.webm", buffer_timestamps,
                            kInfiniteDuration));

  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
  EXPECT_EQ(DemuxerStream::LIVENESS_LIVE, audio->liveness());
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  EXPECT_EQ(DemuxerStream::LIVENESS_LIVE, video->liveness());
  EXPECT_EQ(212949, demuxer_->GetMemoryUsage());
}

TEST_F(ChunkDemuxerTest, WebMFile_AudioOnly) {
  struct BufferTimestamps buffer_timestamps[] = {
      {kSkip, 0}, {kSkip, 3},  {kSkip, 6},
      {kSkip, 9}, {kSkip, 12}, {kSkip, kSkip},
  };

  ExpectInitMediaLogs(HAS_AUDIO);
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2));

  // Expect duration adjustment since actual duration differs slightly from
  // duration in the init segment.
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2746)));

  ASSERT_TRUE(ParseWebMFile("bear-320x240-audio-only.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2744),
                            HAS_AUDIO));
  EXPECT_EQ(18624, demuxer_->GetMemoryUsage());
}

TEST_F(ChunkDemuxerTest, WebMFile_VideoOnly) {
  struct BufferTimestamps buffer_timestamps[] = {
      {0, kSkip},   {33, kSkip},  {67, kSkip},
      {100, kSkip}, {133, kSkip}, {kSkip, kSkip},
  };

  ExpectInitMediaLogs(HAS_VIDEO);

  // Expect duration adjustment since actual duration differs slightly from
  // duration in the init segment.
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2736)));

  ASSERT_TRUE(ParseWebMFile("bear-320x240-video-only.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2703),
                            HAS_VIDEO));
  EXPECT_EQ(194325, demuxer_->GetMemoryUsage());
}

TEST_F(ChunkDemuxerTest, WebMFile_AltRefFrames) {
  struct BufferTimestamps buffer_timestamps[] = {
      {0, 0}, {33, 3}, {33, 6}, {67, 9}, {100, 12}, {kSkip, kSkip},
  };

  ExpectInitMediaLogs(HAS_AUDIO | HAS_VIDEO);
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2));
  ASSERT_TRUE(ParseWebMFile("bear-320x240-altref.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2767)));
}

// Verify that we output buffers before the entire cluster has been parsed.
TEST_F(ChunkDemuxerTest, IncrementalClusterParsing) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  std::unique_ptr<Cluster> cluster(GenerateCluster(0, 6));

  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &video_read_done));

  // Make sure the reads haven't completed yet.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Append data one byte at a time until one or both reads complete.
  int i = 0;
  for (; i < cluster->size() && !(audio_read_done || video_read_done); ++i) {
    ASSERT_TRUE(AppendData(cluster->data() + i, 1));
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(audio_read_done || video_read_done);
  EXPECT_GT(i, 0);
  EXPECT_LT(i, cluster->size());

  audio_read_done = false;
  video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(23),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(33),
                       &video_read_done));

  // Make sure the reads haven't completed yet.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Append the remaining data.
  ASSERT_LT(i, cluster->size());
  ASSERT_TRUE(AppendData(cluster->data() + i, cluster->size() - i));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, ParseErrorDuringInit) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp, CHUNK_DEMUXER_ERROR_APPEND_FAILED),
      true);

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  EXPECT_MEDIA_LOG(StreamParsingFailed());
  uint8_t tmp = 0;
  ASSERT_FALSE(demuxer_->AppendData(
      kSourceId, &tmp, 1, append_window_start_for_next_append_,
      append_window_end_for_next_append_, &timestamp_offset_map_[kSourceId]));
}

TEST_F(ChunkDemuxerTest, AVHeadersWithAudioOnlyType) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp, CHUNK_DEMUXER_ERROR_APPEND_FAILED),
      true);

  ASSERT_EQ(demuxer_->AddId(kSourceId, "audio/webm", "vorbis"),
            ChunkDemuxer::kOk);
  demuxer_->SetTracksWatcher(kSourceId,
                             base::Bind(&ChunkDemuxerTest::InitSegmentReceived,
                                        base::Unretained(this)));

  // Video track is unexpected per mimetype.
  EXPECT_MEDIA_LOG(InitSegmentMismatchesMimeType("Video", "vp8"));
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  ASSERT_FALSE(AppendInitSegment(HAS_AUDIO | HAS_VIDEO));
}

TEST_F(ChunkDemuxerTest, AVHeadersWithVideoOnlyType) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp, CHUNK_DEMUXER_ERROR_APPEND_FAILED),
      true);

  ASSERT_EQ(demuxer_->AddId(kSourceId, "video/webm", "vp8"), ChunkDemuxer::kOk);
  demuxer_->SetTracksWatcher(kSourceId,
                             base::Bind(&ChunkDemuxerTest::InitSegmentReceived,
                                        base::Unretained(this)));

  // Audio track is unexpected per mimetype.
  EXPECT_MEDIA_LOG(FoundStream("video"));
  EXPECT_MEDIA_LOG(CodecName("video", "vp8"));
  EXPECT_MEDIA_LOG(InitSegmentMismatchesMimeType("Audio", "vorbis"));
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  ASSERT_FALSE(AppendInitSegment(HAS_AUDIO | HAS_VIDEO));
}

TEST_F(ChunkDemuxerTest, AudioOnlyHeaderWithAVType) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp, CHUNK_DEMUXER_ERROR_APPEND_FAILED),
      true);

  ASSERT_EQ(demuxer_->AddId(kSourceId, "video/webm", "vorbis,vp8"),
            ChunkDemuxer::kOk);
  demuxer_->SetTracksWatcher(kSourceId,
                             base::Bind(&ChunkDemuxerTest::InitSegmentReceived,
                                        base::Unretained(this)));

  // Video track is also expected per mimetype.
  EXPECT_MEDIA_LOG(FoundStream("audio"));
  EXPECT_MEDIA_LOG(CodecName("audio", "vorbis"));
  EXPECT_MEDIA_LOG(InitSegmentMissesExpectedTrack("vp8"));
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  ASSERT_FALSE(AppendInitSegment(HAS_AUDIO));
}

TEST_F(ChunkDemuxerTest, VideoOnlyHeaderWithAVType) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp, CHUNK_DEMUXER_ERROR_APPEND_FAILED),
      true);

  ASSERT_EQ(demuxer_->AddId(kSourceId, "video/webm", "vorbis,vp8"),
            ChunkDemuxer::kOk);
  demuxer_->SetTracksWatcher(kSourceId,
                             base::Bind(&ChunkDemuxerTest::InitSegmentReceived,
                                        base::Unretained(this)));

  // Audio track is also expected per mimetype.
  EXPECT_MEDIA_LOG(FoundStream("video"));
  EXPECT_MEDIA_LOG(CodecName("video", "vp8"));
  EXPECT_MEDIA_LOG(InitSegmentMissesExpectedTrack("vorbis"));
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  ASSERT_FALSE(AppendInitSegment(HAS_VIDEO));
}

TEST_F(ChunkDemuxerTest, MultipleHeaders) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  // Append another identical initialization segment.
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendInitSegment(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultSecondCluster()));

  GenerateExpectedReads(0, 9);
}

TEST_F(ChunkDemuxerTest, AddSeparateSourcesForAudioAndVideo) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Append audio and video data into separate source ids.
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration)));
  GenerateAudioStreamExpectedReads(0, 4);
  ASSERT_TRUE(AppendCluster(video_id,
                            GenerateSingleStreamCluster(0, 132, kVideoTrackNum,
                                                        kVideoBlockDuration)));
  GenerateVideoStreamExpectedReads(0, 4);
}

TEST_F(ChunkDemuxerTest, AddSeparateSourcesForAudioAndVideoText) {
  // TODO(matthewjheaney): Here and elsewhere, we need more tests
  // for inband text tracks (http://crbug/321455).

  std::string audio_id = "audio1";
  std::string video_id = "video1";

  EXPECT_CALL(host_, AddTextStream(_, _)).Times(Exactly(2));
  ASSERT_TRUE(InitDemuxerAudioAndVideoSourcesText(audio_id, video_id, true));

  // Append audio and video data into separate source ids.
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration)));
  GenerateAudioStreamExpectedReads(0, 4);
  ASSERT_TRUE(AppendCluster(video_id,
                            GenerateSingleStreamCluster(0, 132, kVideoTrackNum,
                                                        kVideoBlockDuration)));
  GenerateVideoStreamExpectedReads(0, 4);
}

TEST_F(ChunkDemuxerTest, AddIdFailures) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_,
                       CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK), true);

  std::string audio_id = "audio1";
  std::string video_id = "video1";

  ASSERT_EQ(AddId(audio_id, HAS_AUDIO), ChunkDemuxer::kOk);

  ExpectInitMediaLogs(HAS_AUDIO);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendInitSegmentWithSourceId(audio_id, HAS_AUDIO));

  // Adding an id after append should fail.
  ASSERT_EQ(AddId(video_id, HAS_VIDEO), ChunkDemuxer::kReachedIdLimit);
}

// Test that Read() calls after a RemoveId() return "end of stream" buffers.
TEST_F(ChunkDemuxerTest, RemoveId) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Append audio and video data into separate source ids.
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(video_id,
                            GenerateSingleStreamCluster(0, 132, kVideoTrackNum,
                                                        kVideoBlockDuration)));

  // Read() from audio should return normal buffers.
  GenerateAudioStreamExpectedReads(0, 4);

  // Audio stream will become inaccessible after |audio_id| is removed, so save
  // it here to read from it after RemoveId.
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Remove the audio id.
  demuxer_->RemoveId(audio_id);

  // Read() from audio should return "end of stream" buffers.
  bool audio_read_done = false;
  audio_stream->Read(base::Bind(&OnReadDone_EOSExpected, &audio_read_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(audio_read_done);

  // Read() from video should still return normal buffers.
  GenerateVideoStreamExpectedReads(0, 4);
}

// Test that removing an ID immediately after adding it does not interfere with
// quota for new IDs in the future.
TEST_F(ChunkDemuxerTest, RemoveAndAddId) {
  std::string audio_id_1 = "audio1";
  ASSERT_TRUE(AddId(audio_id_1, HAS_AUDIO) == ChunkDemuxer::kOk);
  demuxer_->RemoveId(audio_id_1);

  std::string audio_id_2 = "audio2";
  ASSERT_TRUE(AddId(audio_id_2, HAS_AUDIO) == ChunkDemuxer::kOk);
}

TEST_F(ChunkDemuxerTest, SeekCanceled) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Append cluster at the beginning of the stream.
  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 4)));

  // Seek to an unbuffered region.
  Seek(base::TimeDelta::FromSeconds(50));

  // Attempt to read in unbuffered area; should not fulfill the read.
  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_AbortExpected, &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone_AbortExpected, &video_read_done));
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Now cancel the pending seek, which should flush the reads with empty
  // buffers.
  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(0);
  demuxer_->CancelPendingSeek(seek_time);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // A seek back to the buffered region should succeed.
  Seek(seek_time);
  GenerateExpectedReads(0, 4);
}

TEST_F(ChunkDemuxerTest, SeekCanceledWhileWaitingForSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Append cluster at the beginning of the stream.
  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 4)));

  // Start waiting for a seek.
  base::TimeDelta seek_time1 = base::TimeDelta::FromSeconds(50);
  base::TimeDelta seek_time2 = base::TimeDelta::FromSeconds(0);
  demuxer_->StartWaitingForSeek(seek_time1);

  // Now cancel the upcoming seek to an unbuffered region.
  demuxer_->CancelPendingSeek(seek_time2);
  demuxer_->Seek(seek_time1, NewExpectedStatusCB(PIPELINE_OK));

  // Read requests should be fulfilled with empty buffers.
  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_AbortExpected, &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone_AbortExpected, &video_read_done));
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // A seek back to the buffered region should succeed.
  Seek(seek_time2);
  GenerateExpectedReads(0, 4);
}

// Test that Seek() successfully seeks to all source IDs.
TEST_F(ChunkDemuxerTest, SeekAudioAndVideoSources) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(video_id,
                            GenerateSingleStreamCluster(0, 132, kVideoTrackNum,
                                                        kVideoBlockDuration)));

  // Read() should return buffers at 0.
  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(0),
                       &video_read_done));
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // Seek to 3 (an unbuffered region).
  Seek(base::TimeDelta::FromSeconds(3));

  audio_read_done = false;
  video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromSeconds(3),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromSeconds(3),
                       &video_read_done));
  // Read()s should not return until after data is appended at the Seek point.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(3000, 3092, kAudioTrackNum,
                                            kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(3000, 3132, kVideoTrackNum,
                                            kVideoBlockDuration)));

  base::RunLoop().RunUntilIdle();

  // Read() should return buffers at 3.
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

// Test that Seek() completes successfully when EndOfStream
// is called before data is available for that seek point.
// This scenario might be useful if seeking past the end of stream
// of either audio or video (or both).
TEST_F(ChunkDemuxerTest, EndOfStreamAfterPastEosSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum,
                      "0K 10K 20K 30K 40K 50K 60K 70K 80K 90K 100K 110K", 10),
      MuxedStreamInfo(kVideoTrackNum, "0K 20K 40K 60K 80K", 20));
  CheckExpectedRanges("{ [0,100) }");

  // Seeking past the end of video.
  // Note: audio data is available for that seek point.
  bool seek_cb_was_called = false;
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(110);
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time,
                 base::Bind(OnSeekDone_OKExpected, &seek_cb_was_called));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(120)));
  MarkEndOfStream(PIPELINE_OK);
  CheckExpectedRanges("{ [0,120) }");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(seek_cb_was_called);

  ShutdownDemuxer();
}

// Test that EndOfStream is ignored if coming during a pending seek
// whose seek time is before some existing ranges.
TEST_F(ChunkDemuxerTest, EndOfStreamDuringPendingSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum,
                      "0K 10K 20K 30K 40K 50K 60K 70K 80K 90K 100K 110K", 10),
      MuxedStreamInfo(kVideoTrackNum, "0K 20K 40K 60K 80K", 20));
  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum,
                      "200K 210K 220K 230K 240K 250K 260K 270K 280K 290K", 10),
      MuxedStreamInfo(kVideoTrackNum, "200K 220K 240K 260K 280K", 20));

  bool seek_cb_was_called = false;
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(160);
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time,
                 base::Bind(OnSeekDone_OKExpected, &seek_cb_was_called));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(300)));
  MarkEndOfStream(PIPELINE_OK);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  demuxer_->UnmarkEndOfStream();

  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum, "140K 150K 160K 170K", 10),
      MuxedStreamInfo(kVideoTrackNum, "140K 145K 150K 155K 160K 165K 170K 175K",
                      20));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(seek_cb_was_called);

  ShutdownDemuxer();
}

// Test ranges in an audio-only stream.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioIdOnly) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));

  // Test a simple cluster.
  ASSERT_TRUE(AppendCluster(
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration)));

  CheckExpectedRanges("{ [0,92) }");

  // Append a disjoint cluster to check for two separate ranges.
  ASSERT_TRUE(AppendCluster(GenerateSingleStreamCluster(
      150, 219, kAudioTrackNum, kAudioBlockDuration)));

  CheckExpectedRanges("{ [0,92) [150,219) }");
}

// Test ranges in a video-only stream.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_VideoIdOnly) {
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));

  // Test a simple cluster.
  ASSERT_TRUE(AppendCluster(GenerateSingleStreamCluster(0, 132, kVideoTrackNum,
                                                        kVideoBlockDuration)));

  CheckExpectedRanges("{ [0,132) }");

  // Append a disjoint cluster to check for two separate ranges.
  ASSERT_TRUE(AppendCluster(GenerateSingleStreamCluster(
      200, 299, kVideoTrackNum, kVideoBlockDuration)));

  CheckExpectedRanges("{ [0,132) [200,299) }");
}

TEST_F(ChunkDemuxerTest, GetBufferedRanges_SeparateStreams) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Append audio and video data into separate source ids.

  // Audio block: 0 -> 23
  // Video block: 0 -> 33
  // Buffered Range: 0 -> 23
  // Audio block duration is smaller than video block duration,
  // so the buffered ranges should correspond to the audio blocks.
  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(0, 23, kAudioTrackNum, 23)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(0, 33, kVideoTrackNum, 33)));
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,23) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,33) }");
  CheckExpectedRangesForMediaSource("{ [0,23) }");

  // Audio blocks: 300 -> 400
  // Video blocks: 320 -> 420
  // Buffered Range: 320 -> 400  (jagged start and end across SourceBuffers)
  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(300, 400, kAudioTrackNum, 50)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(320, 420, kVideoTrackNum, 50)));
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,23) [300,400) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,33) [320,420) }");
  CheckExpectedRangesForMediaSource("{ [0,23) [320,400) }");

  // Audio block: 520 -> 590
  // Video block: 500 -> 570
  // Buffered Range: 520 -> 570  (jagged start and end across SourceBuffers)
  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(520, 590, kAudioTrackNum, 70)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(500, 570, kVideoTrackNum, 70)));
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,23) [300,400) [520,590) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,33) [320,420) [500,570) }");
  CheckExpectedRangesForMediaSource("{ [0,23) [320,400) [520,570) }");

  // Audio block: 720 -> 750
  // Video block: 700 -> 770
  // Buffered Range: 720 -> 750  (complete overlap of audio)
  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(720, 750, kAudioTrackNum, 30)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(700, 770, kVideoTrackNum, 70)));
  CheckExpectedRanges(DemuxerStream::AUDIO,
                      "{ [0,23) [300,400) [520,590) [720,750) }");
  CheckExpectedRanges(DemuxerStream::VIDEO,
                      "{ [0,33) [320,420) [500,570) [700,770) }");
  CheckExpectedRangesForMediaSource("{ [0,23) [320,400) [520,570) [720,750) }");

  // Audio block: 900 -> 970
  // Video block: 920 -> 950
  // Buffered Range: 920 -> 950  (complete overlap of video)
  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(900, 970, kAudioTrackNum, 70)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(920, 950, kVideoTrackNum, 30)));
  CheckExpectedRanges(DemuxerStream::AUDIO,
                      "{ [0,23) [300,400) [520,590) [720,750) [900,970) }");
  CheckExpectedRanges(DemuxerStream::VIDEO,
                      "{ [0,33) [320,420) [500,570) [700,770) [920,950) }");
  CheckExpectedRangesForMediaSource(
      "{ [0,23) [320,400) [520,570) [720,750) [920,950) }");

  // Appending within buffered range should not affect buffered ranges.
  EXPECT_MEDIA_LOG(GeneratedSplice(40000, 930000));
  ASSERT_TRUE(AppendCluster(
      audio_id, GenerateSingleStreamCluster(930, 950, kAudioTrackNum, 20)));
  ASSERT_TRUE(AppendCluster(
      video_id, GenerateSingleStreamCluster(930, 950, kVideoTrackNum, 20)));
  CheckExpectedRanges(DemuxerStream::AUDIO,
                      "{ [0,23) [300,400) [520,590) [720,750) [900,970) }");
  CheckExpectedRanges(DemuxerStream::VIDEO,
                      "{ [0,33) [320,420) [500,570) [700,770) [920,950) }");
  CheckExpectedRangesForMediaSource(
      "{ [0,23) [320,400) [520,570) [720,750) [920,950) }");
}

TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioVideo) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Audio block: 0 -> 23
  // Video block: 0 -> 33
  // Buffered Range: 0 -> 23
  // Audio block duration is smaller than video block duration,
  // so the buffered ranges should correspond to the audio blocks.
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0D23K"),
                     MuxedStreamInfo(kVideoTrackNum, "0D33K"));

  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,23) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,33) }");
  CheckExpectedRanges("{ [0,23) }");

  // Audio blocks: 300 -> 400
  // Video blocks: 320 -> 420
  // Naive Buffered Range: 320 -> 400  (end overlap) **
  // **Except these are in the same cluster, with same segment start time of
  // 300, so the added buffered range is 300 -> 400  (still with end overlap)
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "300K 350D50K"),
                     MuxedStreamInfo(kVideoTrackNum, "320K 370D50K"));

  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,23) [300,400) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,33) [300,420) }");
  CheckExpectedRanges("{ [0,23) [300,400) }");

  // Audio block: 520 -> 590
  // Video block: 500 -> 570
  // Naive Buffered Range: 520 -> 570  (front overlap) **
  // **Except these are in the same cluster, with same segment start time of
  // 500, so the added buffered range is 500 -> 570
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "520D70K"),
                     MuxedStreamInfo(kVideoTrackNum, "500D70K"));

  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,23) [300,400) [500,590) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,33) [300,420) [500,570) }");
  CheckExpectedRanges("{ [0,23) [300,400) [500,570) }");

  // Audio block: 720 -> 750
  // Video block: 700 -> 770
  // Naive Buffered Range: 720 -> 750  (complete overlap, audio) **
  // **Except these are in the same cluster, with same segment start time of
  // 700, so the added buffered range is 700 -> 750
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "720D30K"),
                     MuxedStreamInfo(kVideoTrackNum, "700D70K"));

  CheckExpectedRanges(DemuxerStream::AUDIO,
                      "{ [0,23) [300,400) [500,590) [700,750) }");
  CheckExpectedRanges(DemuxerStream::VIDEO,
                      "{ [0,33) [300,420) [500,570) [700,770) }");
  CheckExpectedRanges("{ [0,23) [300,400) [500,570) [700,750) }");

  // Audio block: 900 -> 970
  // Video block: 920 -> 950
  // Naive Buffered Range: 920 -> 950  (complete overlap, video) **
  // **Except these are in the same cluster, with same segment start time of
  // 900, so the added buffered range is 900 -> 950
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "900D70K"),
                     MuxedStreamInfo(kVideoTrackNum, "920D30K"));

  CheckExpectedRanges(DemuxerStream::AUDIO,
                      "{ [0,23) [300,400) [500,590) [700,750) [900,970) }");
  CheckExpectedRanges(DemuxerStream::VIDEO,
                      "{ [0,33) [300,420) [500,570) [700,770) [900,950) }");
  CheckExpectedRanges("{ [0,23) [300,400) [500,570) [700,750) [900,950) }");

  // Appending within buffered range should not affect buffered ranges.
  EXPECT_MEDIA_LOG(GeneratedSplice(40000, 930000));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "930D20K"),
                     MuxedStreamInfo(kVideoTrackNum, "930D20K"));
  CheckExpectedRanges(DemuxerStream::AUDIO,
                      "{ [0,23) [300,400) [500,590) [700,750) [900,970) }");
  CheckExpectedRanges(DemuxerStream::VIDEO,
                      "{ [0,33) [300,420) [500,570) [700,770) [900,950) }");
  CheckExpectedRanges("{ [0,23) [300,400) [500,570) [700,750) [900,950) }");
}

TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioVideoText) {
  EXPECT_CALL(host_, AddTextStream(_, _));
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO | HAS_TEXT));

  // Append audio & video data
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0K 23K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "0K 33", 33));

  // Verify that a text track with no cues does not result in an empty buffered
  // range.
  CheckExpectedRanges("{ [0,46) }");

  // Add some text cues.
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "100K 123K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "100K 133", 33),
                     MuxedStreamInfo(kTextTrackNum, "100K 200K"));

  // Verify that the text cues are not reflected in the buffered ranges.
  CheckExpectedRanges("{ [0,46) [100,146) }");

  // Remove the buffered ranges.
  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(250));
  CheckExpectedRanges("{ }");
}

// Once MarkEndOfStream() is called, GetBufferedRanges should not cut off any
// over-hanging tails at the end of the ranges as this is likely due to block
// duration differences.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_EndOfStream) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0K 23K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "0K 33", 33));

  CheckExpectedRanges("{ [0,46) }");

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(66)));
  MarkEndOfStream(PIPELINE_OK);

  // Verify that the range extends to the end of the video data.
  CheckExpectedRanges("{ [0,66) }");

  // Verify that the range reverts to the intersection when end of stream
  // has been cancelled.
  demuxer_->UnmarkEndOfStream();
  CheckExpectedRanges("{ [0,46) }");

  // Append and remove data so that the 2 streams' end ranges do not overlap.
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(398)));
  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum, "200K 223K", 23),
      MuxedStreamInfo(kVideoTrackNum, "200K 233 266 299 332K 365", 33));

  // At this point, the per-stream ranges are as follows:
  // Audio: [0,46) [200,246)
  // Video: [0,66) [200,398)
  CheckExpectedRanges("{ [0,46) [200,246) }");

  demuxer_->Remove(kSourceId, base::TimeDelta::FromMilliseconds(200),
                   base::TimeDelta::FromMilliseconds(300));

  // At this point, the per-stream ranges are as follows:
  // Audio: [0,46)
  // Video: [0,66) [332,398)
  CheckExpectedRanges("{ [0,46) }");

  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "200K 223K", 23),
                     MuxedStreamInfo(kVideoTrackNum, "200K 233", 33));

  // At this point, the per-stream ranges are as follows:
  // Audio: [0,46) [200,246)
  // Video: [0,66) [200,266) [332,398)
  // NOTE: The last range on each stream do not overlap in time.
  CheckExpectedRanges("{ [0,46) [200,246) }");

  MarkEndOfStream(PIPELINE_OK);

  // NOTE: The last range on each stream gets extended to the highest
  // end timestamp according to the spec. The last audio range gets extended
  // from [200,246) to [200,398) which is why the intersection results in the
  // middle range getting larger AND the new range appearing.
  CheckExpectedRanges("{ [0,46) [200,266) [332,398) }");
}

TEST_F(ChunkDemuxerTest, DifferentStreamTimecodes) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Create a cluster where the video timecode begins 25ms after the audio.
  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 25, 8)));

  Seek(base::TimeDelta::FromSeconds(0));
  GenerateExpectedReads(0, 25, 8);

  // Seek to 5 seconds.
  Seek(base::TimeDelta::FromSeconds(5));

  // Generate a cluster to fulfill this seek, where audio timecode begins 25ms
  // after the video.
  ASSERT_TRUE(AppendCluster(GenerateCluster(5025, 5000, 8)));
  GenerateExpectedReads(5025, 5000, 8);
}

TEST_F(ChunkDemuxerTest, DifferentStreamTimecodesSeparateSources) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Generate two streams where the video stream starts 5ms after the audio
  // stream and append them.
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(25, 4 * kAudioBlockDuration + 25,
                                  kAudioTrackNum, kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(
      video_id,
      GenerateSingleStreamCluster(30, 4 * kVideoBlockDuration + 30,
                                  kVideoTrackNum, kVideoBlockDuration)));

  // Both streams should be able to fulfill a seek to 25.
  Seek(base::TimeDelta::FromMilliseconds(25));
  GenerateAudioStreamExpectedReads(25, 4);
  GenerateVideoStreamExpectedReads(30, 4);
}

TEST_F(ChunkDemuxerTest, DifferentStreamTimecodesOutOfRange) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Generate two streams where the video stream starts 10s after the audio
  // stream and append them.
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, 4 * kAudioBlockDuration + 0,
                                  kAudioTrackNum, kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(
      video_id,
      GenerateSingleStreamCluster(10000, 4 * kVideoBlockDuration + 10000,
                                  kVideoTrackNum, kVideoBlockDuration)));

  // Should not be able to fulfill a seek to 0.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(0);
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time, NewExpectedStatusCB(PIPELINE_ERROR_ABORT));
  ExpectRead(DemuxerStream::AUDIO, 0);
  ExpectEndOfStream(DemuxerStream::VIDEO);
}

TEST_F(ChunkDemuxerTest, CodecPrefixMatching) {
  ChunkDemuxer::Status expected = ChunkDemuxer::kNotSupported;

#if defined(USE_PROPRIETARY_CODECS)
#if defined(OS_ANDROID)
  if (HasPlatformDecoderSupport()) expected = ChunkDemuxer::kOk;
#else
  expected = ChunkDemuxer::kOk;
#endif
#endif

  EXPECT_EQ(demuxer_->AddId("source_id", "video/mp4", "avc1.4D4041"), expected);
}

// Test codec ID's that are not compliant with RFC6381, but have been
// seen in the wild.
TEST_F(ChunkDemuxerTest, CodecIDsThatAreNotRFC6381Compliant) {
  ChunkDemuxer::Status expected = ChunkDemuxer::kNotSupported;

#if defined(USE_PROPRIETARY_CODECS)
  expected = ChunkDemuxer::kOk;
#endif
  const char* codec_ids[] = {
      // GPAC places leading zeros on the audio object type.
      "mp4a.40.02", "mp4a.40.05"};

  for (size_t i = 0; i < arraysize(codec_ids); ++i) {
    ChunkDemuxer::Status result =
        demuxer_->AddId("source_id", "audio/mp4", codec_ids[i]);

    EXPECT_EQ(result, expected) << "Fail to add codec_id '" << codec_ids[i]
                                << "'";

    if (result == ChunkDemuxer::kOk) demuxer_->RemoveId("source_id");
  }
}

TEST_F(ChunkDemuxerTest, EndOfStreamStillSetAfterSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());

  base::TimeDelta kLastAudioTimestamp = base::TimeDelta::FromMilliseconds(92);
  base::TimeDelta kLastVideoTimestamp = base::TimeDelta::FromMilliseconds(99);

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));
  ASSERT_TRUE(AppendCluster(kDefaultSecondCluster()));
  MarkEndOfStream(PIPELINE_OK);

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  // Verify that we can read audio & video to the end w/o problems.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastAudioTimestamp, last_timestamp);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastVideoTimestamp, last_timestamp);

  // Seek back to 0 and verify that we can read to the end again..
  Seek(base::TimeDelta::FromMilliseconds(0));

  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastAudioTimestamp, last_timestamp);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastVideoTimestamp, last_timestamp);
}

TEST_F(ChunkDemuxerTest, GetBufferedRangesBeforeInitSegment) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_, base::Bind(&ChunkDemuxerTest::DemuxerInitialized,
                                          base::Unretained(this)),
                       true);
  ASSERT_EQ(AddId("audio", HAS_AUDIO), ChunkDemuxer::kOk);
  ASSERT_EQ(AddId("video", HAS_VIDEO), ChunkDemuxer::kOk);

  CheckExpectedRanges("audio", "{ }");
  CheckExpectedRanges("video", "{ }");
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, EndOfStreamDuringSeek) {
  InSequence s;

  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(0);
  demuxer_->StartWaitingForSeek(seek_time);

  ASSERT_TRUE(AppendCluster(kDefaultSecondCluster()));
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(
                         kDefaultSecondClusterEndTimestamp)));
  MarkEndOfStream(PIPELINE_OK);

  demuxer_->Seek(seek_time, NewExpectedStatusCB(PIPELINE_OK));

  GenerateExpectedReads(0, 4);
  GenerateExpectedReads(46, 66, 5);

  EndOfStreamHelper end_of_stream_helper(demuxer_.get());
  end_of_stream_helper.RequestReads();
  end_of_stream_helper.CheckIfReadDonesWereCalled(true);
}

TEST_F(ChunkDemuxerTest, ConfigChange_Video) {
  InSequence s;

  ASSERT_TRUE(InitDemuxerWithConfigChangeData());

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Fetch initial video config and verify it matches what we expect.
  const VideoDecoderConfig& video_config_1 = video->video_decoder_config();
  ASSERT_TRUE(video_config_1.IsValidConfig());
  EXPECT_EQ(video_config_1.natural_size().width(), 320);
  EXPECT_EQ(video_config_1.natural_size().height(), 240);

  ExpectRead(DemuxerStream::VIDEO, 0);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);

  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 501);

  // Fetch the new decoder config.
  const VideoDecoderConfig& video_config_2 = video->video_decoder_config();
  ASSERT_TRUE(video_config_2.IsValidConfig());
  EXPECT_EQ(video_config_2.natural_size().width(), 640);
  EXPECT_EQ(video_config_2.natural_size().height(), 360);

  ExpectRead(DemuxerStream::VIDEO, 527);

  // Read until the next config change.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 760);

  // Get the new config and verify that it matches the first one.
  ASSERT_TRUE(video_config_1.Matches(video->video_decoder_config()));

  ExpectRead(DemuxerStream::VIDEO, 801);

  // Read until the end of the stream just to make sure there aren't any other
  // config changes.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);
}

TEST_F(ChunkDemuxerTest, ConfigChange_Audio) {
  InSequence s;

  ASSERT_TRUE(InitDemuxerWithConfigChangeData());

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Fetch initial audio config and verify it matches what we expect.
  const AudioDecoderConfig& audio_config_1 = audio->audio_decoder_config();
  ASSERT_TRUE(audio_config_1.IsValidConfig());
  EXPECT_EQ(audio_config_1.samples_per_second(), 44100);
  EXPECT_EQ(audio_config_1.extra_data().size(), 3863u);

  ExpectRead(DemuxerStream::AUDIO, 0);

  // The first config change seen is from a splice frame representing an overlap
  // of buffer from config 1 by buffers from config 2.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 524);

  // Fetch the new decoder config.
  const AudioDecoderConfig& audio_config_2 = audio->audio_decoder_config();
  ASSERT_TRUE(audio_config_2.IsValidConfig());
  EXPECT_EQ(audio_config_2.samples_per_second(), 44100);
  EXPECT_EQ(audio_config_2.extra_data().size(), 3935u);

  // The next config change is from a splice frame representing an overlap of
  // buffers from config 2 by buffers from config 1.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 782);
  ASSERT_TRUE(audio_config_1.Matches(audio->audio_decoder_config()));

  // Read until the end of the stream just to make sure there aren't any other
  // config changes.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 2744);
}

TEST_F(ChunkDemuxerTest, ConfigChange_Seek) {
  InSequence s;

  ASSERT_TRUE(InitDemuxerWithConfigChangeData());

  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Fetch initial video config and verify it matches what we expect.
  const VideoDecoderConfig& video_config_1 = video->video_decoder_config();
  ASSERT_TRUE(video_config_1.IsValidConfig());
  EXPECT_EQ(video_config_1.natural_size().width(), 320);
  EXPECT_EQ(video_config_1.natural_size().height(), 240);

  ExpectRead(DemuxerStream::VIDEO, 0);

  // Seek to a location with a different config.
  Seek(base::TimeDelta::FromMilliseconds(527));

  // Verify that the config change is signalled.
  ExpectConfigChanged(DemuxerStream::VIDEO);

  // Fetch the new decoder config and verify it is what we expect.
  const VideoDecoderConfig& video_config_2 = video->video_decoder_config();
  ASSERT_TRUE(video_config_2.IsValidConfig());
  EXPECT_EQ(video_config_2.natural_size().width(), 640);
  EXPECT_EQ(video_config_2.natural_size().height(), 360);

  // Verify that Read() will return a buffer now.
  ExpectRead(DemuxerStream::VIDEO, 527);

  // Seek back to the beginning and verify we get another config change.
  Seek(base::TimeDelta::FromMilliseconds(0));
  ExpectConfigChanged(DemuxerStream::VIDEO);
  ASSERT_TRUE(video_config_1.Matches(video->video_decoder_config()));
  ExpectRead(DemuxerStream::VIDEO, 0);

  // Seek to a location that requires a config change and then
  // seek to a new location that has the same configuration as
  // the start of the file without a Read() in the middle.
  Seek(base::TimeDelta::FromMilliseconds(527));
  Seek(base::TimeDelta::FromMilliseconds(801));

  // Verify that no config change is signalled.
  ExpectRead(DemuxerStream::VIDEO, 801);
  ASSERT_TRUE(video_config_1.Matches(video->video_decoder_config()));
}

TEST_F(ChunkDemuxerTest, TimestampPositiveOffset) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(SetTimestampOffset(kSourceId, base::TimeDelta::FromSeconds(30)));
  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 2)));

  Seek(base::TimeDelta::FromMilliseconds(30000));

  GenerateExpectedReads(30000, 2);
}

TEST_F(ChunkDemuxerTest, TimestampNegativeOffset) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(SetTimestampOffset(kSourceId, base::TimeDelta::FromSeconds(-1)));
  ASSERT_TRUE(AppendCluster(GenerateCluster(1000, 2)));

  GenerateExpectedReads(0, 2);
}

TEST_F(ChunkDemuxerTest, TimestampOffsetSeparateStreams) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  ASSERT_TRUE(
      SetTimestampOffset(audio_id, base::TimeDelta::FromMilliseconds(-2500)));
  ASSERT_TRUE(
      SetTimestampOffset(video_id, base::TimeDelta::FromMilliseconds(-2500)));
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(2500, 2500 + kAudioBlockDuration * 4,
                                  kAudioTrackNum, kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(
      video_id,
      GenerateSingleStreamCluster(2500, 2500 + kVideoBlockDuration * 4,
                                  kVideoTrackNum, kVideoBlockDuration)));
  GenerateAudioStreamExpectedReads(0, 4);
  GenerateVideoStreamExpectedReads(0, 4);

  Seek(base::TimeDelta::FromMilliseconds(27300));

  ASSERT_TRUE(
      SetTimestampOffset(audio_id, base::TimeDelta::FromMilliseconds(27300)));
  ASSERT_TRUE(
      SetTimestampOffset(video_id, base::TimeDelta::FromMilliseconds(27300)));
  ASSERT_TRUE(AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, kAudioBlockDuration * 4, kAudioTrackNum,
                                  kAudioBlockDuration)));
  ASSERT_TRUE(AppendCluster(
      video_id,
      GenerateSingleStreamCluster(0, kVideoBlockDuration * 4, kVideoTrackNum,
                                  kVideoBlockDuration)));
  GenerateVideoStreamExpectedReads(27300, 4);
  GenerateAudioStreamExpectedReads(27300, 4);
}

TEST_F(ChunkDemuxerTest, IsParsingMediaSegmentMidMediaSegment) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  std::unique_ptr<Cluster> cluster = GenerateCluster(0, 2);
  // Append only part of the cluster data.
  ASSERT_TRUE(AppendData(cluster->data(), cluster->size() - 13));

  // Confirm we're in the middle of parsing a media segment.
  ASSERT_TRUE(demuxer_->IsParsingMediaSegment(kSourceId));

  demuxer_->ResetParserState(kSourceId, append_window_start_for_next_append_,
                             append_window_end_for_next_append_,
                             &timestamp_offset_map_[kSourceId]);

  // After ResetParserState(), parsing should no longer be in the middle of a
  // media segment.
  ASSERT_FALSE(demuxer_->IsParsingMediaSegment(kSourceId));
}

#if defined(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
namespace {
const char* kMp2tMimeType = "video/mp2t";
const char* kMp2tCodecs = "mp4a.40.2,avc1.640028";
}

TEST_F(ChunkDemuxerTest, EmitBuffersDuringAbort) {
  EXPECT_CALL(*this, DemuxerOpened());
  EXPECT_MEDIA_LOG(FoundStream("audio"));
  EXPECT_MEDIA_LOG(CodecName("audio", "aac"));
  EXPECT_MEDIA_LOG(FoundStream("video"));
  EXPECT_MEDIA_LOG(CodecName("video", "h264"));
  demuxer_->Initialize(&host_, CreateInitDoneCB(kInfiniteDuration, PIPELINE_OK),
                       true);
  EXPECT_EQ(ChunkDemuxer::kOk, AddId(kSourceId, kMp2tMimeType, kMp2tCodecs));

  // For info:
  // DTS/PTS derived using dvbsnoop -s ts -if bear-1280x720.ts -tssubdecode
  // Video: first PES:
  //        PTS: 126912 (0x0001efc0)  [= 90 kHz-Timestamp: 0:00:01.4101]
  //        DTS: 123909 (0x0001e405)  [= 90 kHz-Timestamp: 0:00:01.3767]
  // Audio: first PES:
  //        PTS: 126000 (0x0001ec30)  [= 90 kHz-Timestamp: 0:00:01.4000]
  //        DTS: 123910 (0x0001e406)  [= 90 kHz-Timestamp: 0:00:01.3767]
  // Video: last PES:
  //        PTS: 370155 (0x0005a5eb)  [= 90 kHz-Timestamp: 0:00:04.1128]
  //        DTS: 367152 (0x00059a30)  [= 90 kHz-Timestamp: 0:00:04.0794]
  // Audio: last PES:
  //        PTS: 353788 (0x000565fc)  [= 90 kHz-Timestamp: 0:00:03.9309]

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear-1280x720.ts");
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendData(kSourceId, buffer->data(), buffer->data_size()));

  // Confirm we're in the middle of parsing a media segment.
  ASSERT_TRUE(demuxer_->IsParsingMediaSegment(kSourceId));

  // ResetParserState on the Mpeg2 TS parser triggers the emission of the last
  // video buffer which is pending in the stream parser.
  Ranges<base::TimeDelta> range_before_abort =
      demuxer_->GetBufferedRanges(kSourceId);
  demuxer_->ResetParserState(kSourceId, append_window_start_for_next_append_,
                             append_window_end_for_next_append_,
                             &timestamp_offset_map_[kSourceId]);
  Ranges<base::TimeDelta> range_after_abort =
      demuxer_->GetBufferedRanges(kSourceId);

  ASSERT_EQ(range_before_abort.size(), 1u);
  ASSERT_EQ(range_after_abort.size(), 1u);
  EXPECT_EQ(range_after_abort.start(0), range_before_abort.start(0));
  EXPECT_GT(range_after_abort.end(0), range_before_abort.end(0));
}

TEST_F(ChunkDemuxerTest, SeekCompleteDuringAbort) {
  EXPECT_CALL(*this, DemuxerOpened());
  EXPECT_MEDIA_LOG(FoundStream("audio"));
  EXPECT_MEDIA_LOG(CodecName("audio", "aac"));
  EXPECT_MEDIA_LOG(FoundStream("video"));
  EXPECT_MEDIA_LOG(CodecName("video", "h264"));
  demuxer_->Initialize(&host_, CreateInitDoneCB(kInfiniteDuration, PIPELINE_OK),
                       true);
  EXPECT_EQ(ChunkDemuxer::kOk, AddId(kSourceId, kMp2tMimeType, kMp2tCodecs));

  // For info:
  // DTS/PTS derived using dvbsnoop -s ts -if bear-1280x720.ts -tssubdecode
  // Video: first PES:
  //        PTS: 126912 (0x0001efc0)  [= 90 kHz-Timestamp: 0:00:01.4101]
  //        DTS: 123909 (0x0001e405)  [= 90 kHz-Timestamp: 0:00:01.3767]
  // Audio: first PES:
  //        PTS: 126000 (0x0001ec30)  [= 90 kHz-Timestamp: 0:00:01.4000]
  //        DTS: 123910 (0x0001e406)  [= 90 kHz-Timestamp: 0:00:01.3767]
  // Video: last PES:
  //        PTS: 370155 (0x0005a5eb)  [= 90 kHz-Timestamp: 0:00:04.1128]
  //        DTS: 367152 (0x00059a30)  [= 90 kHz-Timestamp: 0:00:04.0794]
  // Audio: last PES:
  //        PTS: 353788 (0x000565fc)  [= 90 kHz-Timestamp: 0:00:03.9309]

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear-1280x720.ts");
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  ASSERT_TRUE(AppendData(kSourceId, buffer->data(), buffer->data_size()));

  // Confirm we're in the middle of parsing a media segment.
  ASSERT_TRUE(demuxer_->IsParsingMediaSegment(kSourceId));

  // Seek to a time corresponding to buffers that will be emitted during the
  // abort.
  Seek(base::TimeDelta::FromMilliseconds(4110));

  // ResetParserState on the Mpeg2 TS parser triggers the emission of the last
  // video buffer which is pending in the stream parser.
  demuxer_->ResetParserState(kSourceId, append_window_start_for_next_append_,
                             append_window_end_for_next_append_,
                             &timestamp_offset_map_[kSourceId]);
}

#endif
#endif

TEST_F(ChunkDemuxerTest, WebMIsParsingMediaSegmentDetection) {
  const uint8_t kBuffer[] = {
      // CLUSTER (size = 10)
      0x1F, 0x43, 0xB6, 0x75, 0x8A,

      // Cluster TIMECODE (value = 1)
      0xE7, 0x81, 0x01,

      // SIMPLEBLOCK (size = 5)
      0xA3, 0x85,

      // Audio Track Number
      0x80 | (kAudioTrackNum & 0x7F),

      // Timecode (relative to cluster) (value = 0)
      0x00, 0x00,

      // Keyframe flag
      0x80,

      // Fake block data
      0x00,

      // CLUSTER (size = unknown; really 10)
      0x1F, 0x43, 0xB6, 0x75, 0xFF,

      // Cluster TIMECODE (value = 2)
      0xE7, 0x81, 0x02,

      // SIMPLEBLOCK (size = 5)
      0xA3, 0x85,

      // Audio Track Number
      0x80 | (kAudioTrackNum & 0x7F),

      // Timecode (relative to cluster) (value = 0)
      0x00, 0x00,

      // Keyframe flag
      0x80,

      // Fake block data
      0x00,

      // EBMLHEADER (size = 10, not fully appended)
      0x1A, 0x45, 0xDF, 0xA3, 0x8A,
  };

  // This array indicates expected return value of IsParsingMediaSegment()
  // following each incrementally appended byte in |kBuffer|.
  const bool kExpectedReturnValues[] = {
      // First Cluster, explicit size
      false, false, false, false, true, true, true, true, true, true, true,
      true, true, true, false,

      // Second Cluster, unknown size
      false, false, false, false, true, true, true, true, true, true, true,
      true, true, true, true,

      // EBMLHEADER
      true, true, true, true, false,
  };

  static_assert(arraysize(kBuffer) == arraysize(kExpectedReturnValues),
                "test arrays out of sync");
  static_assert(arraysize(kBuffer) == sizeof(kBuffer),
                "there should be one byte per index");

  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(23)).Times(2);
  EXPECT_MEDIA_LOG(GeneratedSplice(22000, 2000));
  for (size_t i = 0; i < sizeof(kBuffer); i++) {
    DVLOG(3) << "Appending and testing index " << i;
    ASSERT_TRUE(AppendData(kBuffer + i, 1));
    bool expected_return_value = kExpectedReturnValues[i];
    EXPECT_EQ(expected_return_value,
              demuxer_->IsParsingMediaSegment(kSourceId));
  }
}

TEST_F(ChunkDemuxerTest, DurationChange) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  const int kStreamDuration = kDefaultDuration().InMilliseconds();

  // Add data leading up to the currently set duration.
  ASSERT_TRUE(
      AppendCluster(GenerateCluster(kStreamDuration - kAudioBlockDuration,
                                    kStreamDuration - kVideoBlockDuration, 2)));

  CheckExpectedRanges("{ [201191,201224) }");

  // Add data beginning at the currently set duration and expect a new duration
  // to be signaled. Note that the last video block will have a higher end
  // timestamp than the last audio block.
  const int kNewStreamDurationVideo = kStreamDuration + kVideoBlockDuration;
  EXPECT_CALL(
      host_,
      SetDuration(base::TimeDelta::FromMilliseconds(kNewStreamDurationVideo)));
  ASSERT_TRUE(
      AppendCluster(GenerateCluster(kDefaultDuration().InMilliseconds(), 2)));

  CheckExpectedRanges("{ [201191,201247) }");

  // Add more data to the end of each media type. Note that the last audio block
  // will have a higher end timestamp than the last video block.
  const int kFinalStreamDuration = kStreamDuration + kAudioBlockDuration * 3;
  EXPECT_CALL(
      host_,
      SetDuration(base::TimeDelta::FromMilliseconds(kFinalStreamDuration)));
  ASSERT_TRUE(
      AppendCluster(GenerateCluster(kStreamDuration + kAudioBlockDuration,
                                    kStreamDuration + kVideoBlockDuration, 3)));

  // See that the range has increased appropriately (but not to the full
  // duration of 201293, since there is not enough video appended for that).
  CheckExpectedRanges("{ [201191,201290) }");
}

TEST_F(ChunkDemuxerTest, DurationChangeTimestampOffset) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(SetTimestampOffset(kSourceId, kDefaultDuration()));
  EXPECT_CALL(
      host_, SetDuration(kDefaultDuration() + base::TimeDelta::FromMilliseconds(
                                                  kVideoBlockDuration * 2)));
  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 4)));
}

TEST_F(ChunkDemuxerTest, EndOfStreamTruncateDuration) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(
                         kDefaultFirstClusterEndTimestamp)));
  MarkEndOfStream(PIPELINE_OK);
}

TEST_F(ChunkDemuxerTest, ZeroLengthAppend) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  ASSERT_TRUE(AppendData(NULL, 0));
}

TEST_F(ChunkDemuxerTest, AppendAfterEndOfStream) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());

  ASSERT_TRUE(AppendCluster(kDefaultFirstCluster()));
  MarkEndOfStream(PIPELINE_OK);

  demuxer_->UnmarkEndOfStream();

  ASSERT_TRUE(AppendCluster(kDefaultSecondCluster()));
  MarkEndOfStream(PIPELINE_OK);
}

// Test receiving a Shutdown() call before we get an Initialize()
// call. This can happen if video element gets destroyed before
// the pipeline has a chance to initialize the demuxer.
TEST_F(ChunkDemuxerTest, Shutdown_BeforeInitialize) {
  demuxer_->Shutdown();
  demuxer_->Initialize(&host_, CreateInitDoneCB(DEMUXER_ERROR_COULD_NOT_OPEN),
                       true);
  base::RunLoop().RunUntilIdle();
}

// Verifies that signaling end of stream while stalled at a gap
// boundary does not trigger end of stream buffers to be returned.
TEST_F(ChunkDemuxerTest, EndOfStreamWhileWaitingForGapToBeFilled) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(0, 10));
  ASSERT_TRUE(AppendCluster(300, 10));
  CheckExpectedRanges("{ [0,132) [300,432) }");

  GenerateExpectedReads(0, 10);

  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(138),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone, base::TimeDelta::FromMilliseconds(138),
                       &video_read_done));

  // Verify that the reads didn't complete
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(438)));
  MarkEndOfStream(PIPELINE_OK);

  // Verify that the reads still haven't completed.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  demuxer_->UnmarkEndOfStream();

  ASSERT_TRUE(AppendCluster(138, 22));

  base::RunLoop().RunUntilIdle();

  CheckExpectedRanges("{ [0,435) }");

  // Verify that the reads have completed.
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // Read the rest of the buffers.
  GenerateExpectedReads(161, 171, 20);

  // Verify that reads block because the append cleared the end of stream state.
  audio_read_done = false;
  video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_EOSExpected, &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone_EOSExpected, &video_read_done));

  // Verify that the reads don't complete.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(437)));
  MarkEndOfStream(PIPELINE_OK);

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, CanceledSeekDuringInitialPreroll) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Cancel preroll.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(200);
  demuxer_->CancelPendingSeek(seek_time);

  // Initiate the seek to the new location.
  Seek(seek_time);

  // Append data to satisfy the seek.
  ASSERT_TRUE(AppendCluster(seek_time.InMilliseconds(), 10));
}

TEST_F(ChunkDemuxerTest, SetMemoryLimitType) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Set different memory limits for audio and video.
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::VIDEO, 5 * kBlockSize + 1);

  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(1000);

  // Append data at the start that can be garbage collected:
  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum,
                      "0K 23K 46K 69K 92K 115K 138K 161K 184K 207K", 23),
      MuxedStreamInfo(kVideoTrackNum, "0K 33K 66K 99K 132K", 33));

  // We should be right at buffer limit, should pass
  EXPECT_TRUE(demuxer_->EvictCodedFrames(
      kSourceId, base::TimeDelta::FromMilliseconds(0), 0));

  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,230) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,165) }");

  // Seek so we can garbage collect the data appended above.
  Seek(seek_time);

  // Append data at seek_time.
  AppendMuxedCluster(
      MuxedStreamInfo(
          kAudioTrackNum,
          "1000K 1023K 1046K 1069K 1092K 1115K 1138K 1161K 1184K 1207K", 23),
      MuxedStreamInfo(kVideoTrackNum, "1000K 1033K 1066K 1099K 1132K", 33));

  // We should delete first append, and be exactly at buffer limit
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 0));

  // Verify that the old data, and nothing more, has been garbage collected.
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [1000,1230) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [1000,1165) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek_SingleRange_SeekForward) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  // Append some data at position 1000ms
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1000, 10);
  CheckExpectedRanges("{ [1000,1230) }");

  // GC should be able to evict frames in the currently buffered range, since
  // those frames are earlier than the seek target position.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(2000);
  Seek(seek_time);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 5 * kBlockSize));

  // Append data to complete seek operation
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 2000, 5);
  CheckExpectedRanges("{ [1115,1230) [2000,2115) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek_SingleRange_SeekBack) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  // Append some data at position 1000ms
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1000, 10);
  CheckExpectedRanges("{ [1000,1230) }");

  // GC should be able to evict frames in the currently buffered range, since
  // seek target position has no data and so we should allow some frames to be
  // evicted to make space for the upcoming append at seek target position.
  base::TimeDelta seek_time = base::TimeDelta();
  Seek(seek_time);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 5 * kBlockSize));

  // Append data to complete seek operation
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 0, 5);
  CheckExpectedRanges("{ [0,115) [1115,1230) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek_MultipleRanges_SeekForward) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  // Append some data at position 1000ms then at 2000ms
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1000, 5);
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 2000, 5);
  CheckExpectedRanges("{ [1000,1115) [2000,2115) }");

  // GC should be able to evict frames in the currently buffered ranges, since
  // those frames are earlier than the seek target position.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(3000);
  Seek(seek_time);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 8 * kBlockSize));

  // Append data to complete seek operation
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 3000, 5);
  CheckExpectedRanges("{ [2069,2115) [3000,3115) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek_MultipleRanges_SeekInbetween1) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  // Append some data at position 1000ms then at 2000ms
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1000, 5);
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 2000, 5);
  CheckExpectedRanges("{ [1000,1115) [2000,2115) }");

  // GC should be able to evict all frames from the first buffered range, since
  // those frames are earlier than the seek target position. But there's only 5
  // blocks worth of data in the first range and seek target position has no
  // data, so GC proceeds with trying to delete some frames from the back of
  // buffered ranges, that doesn't yield anything, since that's the most
  // recently appended data, so then GC starts removing data from the front of
  // the remaining buffered range (2000ms) to ensure we free up enough space for
  // the upcoming append and allow seek to proceed.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(1500);
  Seek(seek_time);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 8 * kBlockSize));

  // Append data to complete seek operation
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1500, 5);
  CheckExpectedRanges("{ [1500,1615) [2069,2115) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek_MultipleRanges_SeekInbetween2) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);

  // Append some data at position 2000ms first, then at 1000ms, so that the last
  // appended data position is in the first buffered range (that matters to the
  // GC algorithm since it tries to preserve more recently appended data).
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 2000, 5);
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1000, 5);
  CheckExpectedRanges("{ [1000,1115) [2000,2115) }");

  // Now try performing garbage collection without announcing seek first, i.e.
  // without calling Seek(), the GC algorithm should try to preserve data in the
  // first range, since that is most recently appended data.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(2030);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 5 * kBlockSize));

  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1500, 5);
  CheckExpectedRanges("{ [1000,1115) [1500,1615) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek_MultipleRanges_SeekBack) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  // Append some data at position 1000ms then at 2000ms
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 1000, 5);
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 2000, 5);
  CheckExpectedRanges("{ [1000,1115) [2000,2115) }");

  // GC should be able to evict frames in the currently buffered ranges, since
  // those frames are earlier than the seek target position.
  base::TimeDelta seek_time = base::TimeDelta();
  Seek(seek_time);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time, 8 * kBlockSize));

  // Append data to complete seek operation
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 0, 5);
  CheckExpectedRanges("{ [0,115) [2069,2115) }");
}

TEST_F(ChunkDemuxerTest, GCDuringSeek) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));

  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 5 * kBlockSize);

  base::TimeDelta seek_time1 = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta seek_time2 = base::TimeDelta::FromMilliseconds(500);

  // Initiate a seek to |seek_time1|.
  Seek(seek_time1);

  // Append data to satisfy the first seek request.
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum,
                            seek_time1.InMilliseconds(), 5);
  CheckExpectedRanges("{ [1000,1115) }");

  // We are under memory limit, so Evict should be a no-op.
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time1, 0));
  CheckExpectedRanges("{ [1000,1115) }");

  // Signal that the second seek is starting.
  demuxer_->StartWaitingForSeek(seek_time2);

  // Append data to satisfy the second seek.
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum,
                            seek_time2.InMilliseconds(), 5);
  CheckExpectedRanges("{ [500,615) [1000,1115) }");

  // We are now over our memory usage limit. We have just seeked to |seek_time2|
  // so data around 500ms position should be preserved, while the previous
  // append at 1000ms should be removed.
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time2, 0));
  CheckExpectedRanges("{ [500,615) }");

  // Complete the seek.
  demuxer_->Seek(seek_time2, NewExpectedStatusCB(PIPELINE_OK));

  // Append more data and make sure that we preserve both the buffered range
  // around |seek_time2|, because that's the current playback position,
  // and the newly appended range, since this is the most recent append.
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 700, 5);
  EXPECT_FALSE(demuxer_->EvictCodedFrames(kSourceId, seek_time2, 0));
  CheckExpectedRanges("{ [500,615) [700,815) }");
}

TEST_F(ChunkDemuxerTest, GCKeepPlayhead) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));

  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 5 * kBlockSize);

  // Append data at the start that can be garbage collected:
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 0, 10);
  CheckExpectedRanges("{ [0,230) }");

  // We expect garbage collection to fail, as we don't want to spontaneously
  // create gaps in source buffer stream. Gaps could break playback for many
  // clients, who don't bother to check ranges after append.
  EXPECT_FALSE(demuxer_->EvictCodedFrames(
      kSourceId, base::TimeDelta::FromMilliseconds(0), 0));
  CheckExpectedRanges("{ [0,230) }");

  // Increase media_time a bit, this will allow some data to be collected, but
  // we are still over memory usage limit.
  base::TimeDelta seek_time1 = base::TimeDelta::FromMilliseconds(23 * 2);
  Seek(seek_time1);
  EXPECT_FALSE(demuxer_->EvictCodedFrames(kSourceId, seek_time1, 0));
  CheckExpectedRanges("{ [46,230) }");

  base::TimeDelta seek_time2 = base::TimeDelta::FromMilliseconds(23 * 4);
  Seek(seek_time2);
  EXPECT_FALSE(demuxer_->EvictCodedFrames(kSourceId, seek_time2, 0));
  CheckExpectedRanges("{ [92,230) }");

  // media_time has progressed to a point where we can collect enough data to
  // be under memory limit, so Evict should return true.
  base::TimeDelta seek_time3 = base::TimeDelta::FromMilliseconds(23 * 6);
  Seek(seek_time3);
  EXPECT_TRUE(demuxer_->EvictCodedFrames(kSourceId, seek_time3, 0));
  // Strictly speaking the current playback time is 23*6==138ms, so we could
  // release data up to 138ms, but we only release as much data as necessary
  // to bring memory usage under the limit, so we release only up to 115ms.
  CheckExpectedRanges("{ [115,230) }");
}

TEST_F(ChunkDemuxerTest, AppendWindow_Video) {
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Set the append window to [50,280).
  append_window_start_for_next_append_ = base::TimeDelta::FromMilliseconds(50);
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(280);

  // Append a cluster that starts before and ends after the append window.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(30));
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum,
                            "0K 30 60 90 120K 150 180 210 240K 270 300 330K");

  // Verify that GOPs that start outside the window are not included
  // in the buffer. Also verify that buffers that start inside the
  // window and extend beyond the end of the window are not included.
  CheckExpectedRanges("{ [120,270) }");
  CheckExpectedBuffers(stream, "120K 150 180 210 240K");

  // Extend the append window to [50,650).
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(650);

  // Append more data and verify that adding buffers start at the next
  // key frame.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(30));
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum,
                            "360 390 420K 450 480 510 540K 570 600 630K");
  CheckExpectedRanges("{ [120,270) [420,630) }");
}

TEST_F(ChunkDemuxerTest, AppendWindow_Audio) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Set the append window to [50,280).
  append_window_start_for_next_append_ = base::TimeDelta::FromMilliseconds(50);
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(280);

  // Append a cluster that starts before and ends after the append window.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(30));
  AppendSingleStreamCluster(
      kSourceId, kAudioTrackNum,
      "0K 30K 60K 90K 120K 150K 180K 210K 240K 270K 300K 330K");

  // Verify that frames that end outside the window are not included
  // in the buffer. Also verify that buffers that start inside the
  // window and extend beyond the end of the window are not included.
  //
  // The first 50ms of the range should be truncated since it overlaps
  // the start of the append window.
  CheckExpectedRanges("{ [50,280) }");

  // The "50P" buffer is the "0" buffer marked for complete discard.  The next
  // "50" buffer is the "30" buffer marked with 20ms of start discard.
  CheckExpectedBuffers(stream, "50KP 50K 60K 90K 120K 150K 180K 210K 240K");

  // Extend the append window to [50,650).
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(650);

  // Append more data and verify that a new range is created.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(30));
  AppendSingleStreamCluster(
      kSourceId, kAudioTrackNum,
      "360K 390K 420K 450K 480K 510K 540K 570K 600K 630K");
  CheckExpectedRanges("{ [50,280) [360,650) }");
}

TEST_F(ChunkDemuxerTest, AppendWindow_AudioOverlapStartAndEnd) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));

  // Set the append window to [10,20).
  append_window_start_for_next_append_ = base::TimeDelta::FromMilliseconds(10);
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(20);

  // Append a cluster that starts before and ends after the append window.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(
      WebMClusterParser::kDefaultAudioBufferDurationInMs));
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, "0K");

  // Verify the append is clipped to the append window.
  CheckExpectedRanges("{ [10,20) }");
}

TEST_F(ChunkDemuxerTest, AppendWindow_WebMFile_AudioOnly) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_,
      CreateInitDoneCB(base::TimeDelta::FromMilliseconds(2744), PIPELINE_OK),
      true);
  ASSERT_EQ(ChunkDemuxer::kOk, AddId(kSourceId, HAS_AUDIO));

  // Set the append window to [50,150).
  append_window_start_for_next_append_ = base::TimeDelta::FromMilliseconds(50);
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(150);

  // Read a WebM file into memory and send the data to the demuxer.  The chunk
  // size has been chosen carefully to ensure the preroll buffer used by the
  // partial append window trim must come from a previous Append() call.
  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-320x240-audio-only.webm");
  ExpectInitMediaLogs(HAS_AUDIO);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2));
  ASSERT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 128));

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  CheckExpectedBuffers(stream, "50KP 50K 62K 86K 109K 122K 125K 128K");
}

TEST_F(ChunkDemuxerTest, AppendWindow_AudioConfigUpdateRemovesPreroll) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_,
      CreateInitDoneCB(base::TimeDelta::FromMilliseconds(2744), PIPELINE_OK),
      true);
  ASSERT_EQ(ChunkDemuxer::kOk, AddId(kSourceId, HAS_AUDIO));

  // Set the append window such that the first file is completely before the
  // append window.
  // Expect duration adjustment since actual duration differs slightly from
  // duration in the init segment.
  const base::TimeDelta duration_1 = base::TimeDelta::FromMilliseconds(2746);
  append_window_start_for_next_append_ = duration_1;

  // Read a WebM file into memory and append the data.
  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-320x240-audio-only.webm");
  ExpectInitMediaLogs(HAS_AUDIO);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(2));
  ASSERT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
  CheckExpectedRanges("{ }");

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  AudioDecoderConfig config_1 = stream->audio_decoder_config();

  // Read a second WebM with a different config in and append the data.
  scoped_refptr<DecoderBuffer> buffer2 =
      ReadTestDataFile("bear-320x240-audio-only-48khz.webm");
  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(21));
  EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());
  ASSERT_TRUE(SetTimestampOffset(kSourceId, duration_1));
  ASSERT_TRUE(AppendDataInPieces(buffer2->data(), buffer2->data_size(), 512));
  CheckExpectedRanges("{ [2746,5519) }");

  Seek(duration_1);
  ExpectConfigChanged(DemuxerStream::AUDIO);
  ASSERT_FALSE(config_1.Matches(stream->audio_decoder_config()));
  CheckExpectedBuffers(stream, "2746K 2767K 2789K 2810K");
}

TEST_F(ChunkDemuxerTest, AppendWindow_Text) {
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _)).WillOnce(SaveArg<0>(&text_stream));
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO | HAS_TEXT));
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Set the append window to [20,280).
  append_window_start_for_next_append_ = base::TimeDelta::FromMilliseconds(20);
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(280);

  // Append a cluster that starts before and ends after the append
  // window.
  AppendMuxedCluster(
      MuxedStreamInfo(kVideoTrackNum,
                      "0K 30 60 90 120K 150 180 210 240K 270 300 330K", 30),
      MuxedStreamInfo(kTextTrackNum, "0K 100K 200K 300K"));

  // Verify that text cues that start outside the window are not included
  // in the buffer. Also verify that cues that extend beyond the
  // window are not included.
  CheckExpectedRanges("{ [100,270) }");
  CheckExpectedBuffers(video_stream, "120K 150 180 210 240K");
  CheckExpectedBuffers(text_stream, "100K");

  // Extend the append window to [20,650).
  append_window_end_for_next_append_ = base::TimeDelta::FromMilliseconds(650);

  // Append more data and verify that a new range is created.
  AppendMuxedCluster(
      MuxedStreamInfo(kVideoTrackNum,
                      "360 390 420K 450 480 510 540K 570 600 630K", 30),
      MuxedStreamInfo(kTextTrackNum, "400K 500K 600K 700K"));
  CheckExpectedRanges("{ [100,270) [400,630) }");

  // Seek to the new range and verify that the expected buffers are returned.
  Seek(base::TimeDelta::FromMilliseconds(420));
  CheckExpectedBuffers(video_stream, "420K 450 480 510 540K 570 600");
  CheckExpectedBuffers(text_stream, "400K 500K");
}

TEST_F(ChunkDemuxerTest, StartWaitingForSeekAfterParseError) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  EXPECT_MEDIA_LOG(StreamParsingFailed());
  EXPECT_CALL(host_, OnDemuxerError(CHUNK_DEMUXER_ERROR_APPEND_FAILED));
  AppendGarbage();
  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(50);
  demuxer_->StartWaitingForSeek(seek_time);
}

TEST_F(ChunkDemuxerTest, Remove_AudioVideoText) {
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _)).WillOnce(SaveArg<0>(&text_stream));
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO | HAS_TEXT));

  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum, "0K 20K 40K 60K 80K 100K 120K 140K", 20),
      MuxedStreamInfo(kVideoTrackNum, "0K 30 60 90 120K 150 180", 30),
      MuxedStreamInfo(kTextTrackNum, "0K 100K 200K"));

  CheckExpectedBuffers(audio_stream, "0K 20K 40K 60K 80K 100K 120K 140K");
  CheckExpectedBuffers(video_stream, "0K 30 60 90 120K 150 180");
  CheckExpectedBuffers(text_stream, "0K 100K 200K");

  // Remove the buffers that were added.
  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(300));

  // Verify that all the appended data has been removed.
  CheckExpectedRanges("{ }");

  // Append new buffers that are clearly different than the original
  // ones and verify that only the new buffers are returned.
  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum, "1K 21K 41K 61K 81K 101K 121K 141K", 20),
      MuxedStreamInfo(kVideoTrackNum, "1K 31 61 91 121K 151 181", 30),
      MuxedStreamInfo(kTextTrackNum, "1K 101K 201K"));

  Seek(base::TimeDelta());
  CheckExpectedBuffers(audio_stream, "1K 21K 41K 61K 81K 101K 121K 141K");
  CheckExpectedBuffers(video_stream, "1K 31 61 91 121K 151 181");
  CheckExpectedBuffers(text_stream, "1K 101K 201K");
}

TEST_F(ChunkDemuxerTest, Remove_StartAtDuration) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Set the duration to something small so that the append that
  // follows updates the duration to reflect the end of the appended data.
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(1)));
  demuxer_->SetDuration(0.001);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(160)));
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum,
                            "0K 20K 40K 60K 80K 100K 120K 140D20K");

  CheckExpectedRanges("{ [0,160) }");
  CheckExpectedBuffers(audio_stream, "0K 20K 40K 60K 80K 100K 120K 140K");

  demuxer_->Remove(kSourceId,
                   base::TimeDelta::FromSecondsD(demuxer_->GetDuration()),
                   kInfiniteDuration);

  Seek(base::TimeDelta());
  CheckExpectedRanges("{ [0,160) }");
  CheckExpectedBuffers(audio_stream, "0K 20K 40K 60K 80K 100K 120K 140K");
}

// Verifies that a Seek() will complete without text cues for
// the seek point and will return cues after the seek position
// when they are eventually appended.
TEST_F(ChunkDemuxerTest, SeekCompletesWithoutTextCues) {
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _)).WillOnce(SaveArg<0>(&text_stream));
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO | HAS_TEXT));

  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(120);
  bool seek_cb_was_called = false;
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time,
                 base::Bind(OnSeekDone_OKExpected, &seek_cb_was_called));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  bool text_read_done = false;
  text_stream->Read(base::Bind(
      &OnReadDone, base::TimeDelta::FromMilliseconds(225), &text_read_done));

  // Append audio & video data so the seek completes.
  AppendMuxedCluster(
      MuxedStreamInfo(kAudioTrackNum,
                      "0K 20K 40K 60K 80K 100K 120K 140K 160K 180K 200K", 20),
      MuxedStreamInfo(kVideoTrackNum, "0K 30 60 90 120K 150 180 210", 30));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(seek_cb_was_called);
  EXPECT_FALSE(text_read_done);

  // Read some audio & video buffers to further verify seek completion.
  CheckExpectedBuffers(audio_stream, "120K 140K");
  CheckExpectedBuffers(video_stream, "120K 150");

  EXPECT_FALSE(text_read_done);

  // Append text cues that start after the seek point and verify that
  // they are returned by Read() calls.
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "220K 240K 260K 280K", 20),
                     MuxedStreamInfo(kVideoTrackNum, "240K 270 300 330", 30),
                     MuxedStreamInfo(kTextTrackNum, "225K 275K 325K"));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(text_read_done);

  // NOTE: we start at 275 here because the buffer at 225 was returned
  // to the pending read initiated above.
  CheckExpectedBuffers(text_stream, "275K 325K");

  // Verify that audio & video streams continue to return expected values.
  CheckExpectedBuffers(audio_stream, "160K 180K");
  CheckExpectedBuffers(video_stream, "180 210");
}

TEST_F(ChunkDemuxerTest, ClusterWithUnknownSize) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 0, 4, true)));
  CheckExpectedRanges("{ [0,46) }");

  // A new cluster indicates end of the previous cluster with unknown size.
  ASSERT_TRUE(AppendCluster(GenerateCluster(46, 66, 5, true)));
  CheckExpectedRanges("{ [0,115) }");
}

TEST_F(ChunkDemuxerTest, CuesBetweenClustersWithUnknownSize) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  // Add two clusters separated by Cues in a single Append() call.
  std::unique_ptr<Cluster> cluster = GenerateCluster(0, 0, 4, true);
  std::vector<uint8_t> data(cluster->data(), cluster->data() + cluster->size());
  data.insert(data.end(), kCuesHeader, kCuesHeader + sizeof(kCuesHeader));
  cluster = GenerateCluster(46, 66, 5, true);
  data.insert(data.end(), cluster->data(), cluster->data() + cluster->size());
  ASSERT_TRUE(AppendData(&*data.begin(), data.size()));

  CheckExpectedRanges("{ [0,115) }");
}

TEST_F(ChunkDemuxerTest, CuesBetweenClusters) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));

  ASSERT_TRUE(AppendCluster(GenerateCluster(0, 0, 4)));
  ASSERT_TRUE(AppendData(kCuesHeader, sizeof(kCuesHeader)));
  ASSERT_TRUE(AppendCluster(GenerateCluster(46, 66, 5)));
  CheckExpectedRanges("{ [0,115) }");
}

TEST_F(ChunkDemuxerTest, EvictCodedFramesTest) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, 10 * kBlockSize);
  demuxer_->SetMemoryLimitsForTest(DemuxerStream::VIDEO, 15 * kBlockSize);
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  const char* kAudioStreamInfo = "0K 40K 80K 120K 160K 200K 240K 280K";
  const char* kVideoStreamInfo =
      "0K 10 20K 30 40K 50 60K 70 80K 90 100K "
      "110 120K 130 140K";
  // Append 8 blocks (80 bytes) of data to audio stream and 15 blocks (150
  // bytes) to video stream.
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, kAudioStreamInfo, 40),
                     MuxedStreamInfo(kVideoTrackNum, kVideoStreamInfo, 10));
  CheckExpectedBuffers(audio_stream, kAudioStreamInfo);
  CheckExpectedBuffers(video_stream, kVideoStreamInfo);

  // If we want to append 80 more blocks of muxed a+v data and the current
  // position is 0, that will fail, because EvictCodedFrames won't remove the
  // data after the current playback position.
  ASSERT_FALSE(demuxer_->EvictCodedFrames(
      kSourceId, base::TimeDelta::FromMilliseconds(0), 80));
  // EvictCodedFrames has failed, so data should be unchanged.
  Seek(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers(audio_stream, kAudioStreamInfo);
  CheckExpectedBuffers(video_stream, kVideoStreamInfo);

  // But if we pretend that playback position has moved to 120ms, that allows
  // EvictCodedFrames to garbage-collect enough data to succeed.
  ASSERT_TRUE(demuxer_->EvictCodedFrames(
      kSourceId, base::TimeDelta::FromMilliseconds(120), 80));

  Seek(base::TimeDelta::FromMilliseconds(0));
  // Audio stream had 8 buffers, video stream had 15. We told EvictCodedFrames
  // that the new data size is 8 blocks muxed, i.e. 80 bytes. Given the current
  // ratio of video to the total data size (15 : (8+15) ~= 0.65) the estimated
  // sizes of video and audio data in the new 80 byte chunk are 52 bytes for
  // video (80*0.65 = 52) and 28 bytes for audio (80 - 52).
  // Given these numbers MSE GC will remove just one audio block (since current
  // audio size is 80 bytes, new data is 28 bytes, we need to remove just one 10
  // byte block to stay under 100 bytes memory limit after append
  // 80 - 10 + 28 = 98).
  // For video stream 150 + 52 = 202. Video limit is 150 bytes. We need to
  // remove at least 6 blocks to stay under limit.
  CheckExpectedBuffers(audio_stream, "40K 80K 120K 160K 200K 240K 280K");
  CheckExpectedBuffers(video_stream, "60K 70 80K 90 100K 110 120K 130 140K");
}

TEST_F(ChunkDemuxerTest, SegmentMissingAudioFrame_AudioOnly) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO));
  EXPECT_MEDIA_LOG(SegmentMissingFrames("2"));
  ASSERT_TRUE(AppendCluster(GenerateEmptyCluster(0)));
}

TEST_F(ChunkDemuxerTest, SegmentMissingVideoFrame_VideoOnly) {
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  EXPECT_MEDIA_LOG(SegmentMissingFrames("1"));
  ASSERT_TRUE(AppendCluster(GenerateEmptyCluster(0)));
}

TEST_F(ChunkDemuxerTest, SegmentMissingAudioFrame_AudioVideo) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  EXPECT_MEDIA_LOG(SegmentMissingFrames("2"));
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, 0, 10);
}

TEST_F(ChunkDemuxerTest, SegmentMissingVideoFrame_AudioVideo) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  EXPECT_MEDIA_LOG(SegmentMissingFrames("1"));
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 0, 10);
}

TEST_F(ChunkDemuxerTest, SegmentMissingAudioVideoFrames) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  EXPECT_MEDIA_LOG(SegmentMissingFrames("1"));
  EXPECT_MEDIA_LOG(SegmentMissingFrames("2"));
  ASSERT_TRUE(AppendCluster(GenerateEmptyCluster(0)));
}

TEST_F(ChunkDemuxerTest, RelaxedKeyframe_FirstSegmentMissingKeyframe) {
  // Append V:[n n n][n n K]
  // Expect V:           [K]
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(10)).Times(2);
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "0 10 20");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "30 40 50K");
  CheckExpectedRanges("{ [50,60) }");
  CheckExpectedBuffers(video_stream, "50K");
}

TEST_F(ChunkDemuxerTest, RelaxedKeyframe_SecondSegmentMissingKeyframe) {
  // Append V:[K n n][n n n]
  // Expect V:[K n n][n n n]
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(10)).Times(2);
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "0K 10 20");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "30 40 50");
  CheckExpectedRanges("{ [0,60) }");
  CheckExpectedBuffers(video_stream, "0K 10 20 30 40 50");
}

TEST_F(ChunkDemuxerTest, RelaxedKeyframe_RemoveInterruptsCodedFrameGroup_1) {
  // Append V:[K n n]
  // Remove    *****
  // Append V:       [n n n][n K n]
  // Expect:                  [K n]
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(10)).Times(3);
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "0K 10 20");
  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(30));
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "30 40 50");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "60 70K 80");
  CheckExpectedRanges("{ [70,90) }");
  CheckExpectedBuffers(video_stream, "70K 80");
}

TEST_F(ChunkDemuxerTest, RelaxedKeyframe_RemoveInterruptsCodedFrameGroup_2) {
  // Append V:[K n n][n n n][n K n]
  // Remove    *
  // Expect:                  [K n]
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(10)).Times(3);
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "0K 10 20");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "30 40 50");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "60 70K 80");
  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(10));
  CheckExpectedRanges("{ [70,90) }");
  CheckExpectedBuffers(video_stream, "70K 80");
}

TEST_F(ChunkDemuxerTest, RelaxedKeyframe_RemoveInterruptsCodedFrameGroup_3) {
  // Append V:[K n n][n n n][n K n]
  // Remove               *
  // Expect:  [K n n..n n]    [K n]
  ASSERT_TRUE(InitDemuxer(HAS_VIDEO));
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(10)).Times(3);
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "0K 10 20");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "30 40 50");
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum, "60 70K 80");
  demuxer_->Remove(kSourceId, base::TimeDelta::FromMilliseconds(50),
                   base::TimeDelta::FromMilliseconds(60));
  CheckExpectedRanges("{ [0,50) [70,90) }");
  CheckExpectedBuffers(video_stream, "0K 10 20 30 40");
  Seek(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers(video_stream, "70K 80");
}

TEST_F(ChunkDemuxerTest,
       RelaxedKeyframe_RemoveInterruptsMuxedCodedFrameGroup_1) {
  // Append muxed:
  //        A:[K K K]
  //        V:[K n n]
  // Remove    *****
  // Append muxed:
  //        A:       [K K K][K K K]
  //        V:       [n n n][n K n]
  // Expect:
  //        A:       [K K K][K K K]
  //        V                 [K n]
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0K 10K 20D10K"),
                     MuxedStreamInfo(kVideoTrackNum, "0K 10 20", 10));
  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(30));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "30K 40K 50D10K"),
                     MuxedStreamInfo(kVideoTrackNum, "30 40 50", 10));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "60K 70K 80D10K"),
                     MuxedStreamInfo(kVideoTrackNum, "60 70K 80", 10));
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [30,90) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [70,90) }");
  CheckExpectedRanges("{ [70,90) }");
  CheckExpectedBuffers(audio_stream, "30K 40K 50K 60K 70K 80K");
  CheckExpectedBuffers(video_stream, "70K 80");
}

TEST_F(ChunkDemuxerTest,
       RelaxedKeyframe_RemoveInterruptsMuxedCodedFrameGroup_2) {
  // Append muxed:
  //        A:[K K K]
  //        V:(Nothing, simulating jagged cluster start or a long previous
  //          video frame)
  // Remove    *****
  // Append muxed:
  //        A:       [K K K][K K K]
  //        V:       [n n n][n K n]
  // Expect:
  //        A:       [K K K][K K K]
  //        V [................K n] (As would occur if there really were a
  //        jagged cluster start and not badly muxed clusters as used to
  //        simulate a jagged start in this test.)
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  EXPECT_MEDIA_LOG(SegmentMissingFrames("1"));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "0K 10K 20D10K"),
                     MuxedStreamInfo(kVideoTrackNum, ""));
  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(30));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "30K 40K 50D10K"),
                     MuxedStreamInfo(kVideoTrackNum, "30 40 50", 10));
  AppendMuxedCluster(MuxedStreamInfo(kAudioTrackNum, "60K 70K 80D10K"),
                     MuxedStreamInfo(kVideoTrackNum, "60 70K 80", 10));
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [30,90) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,90) }");
  CheckExpectedRanges("{ [30,90) }");
  CheckExpectedBuffers(audio_stream, "30K 40K 50K 60K 70K 80K");
  CheckExpectedBuffers(video_stream, "70K 80");
}

TEST_F(ChunkDemuxerTest,
       RelaxedKeyframe_RemoveInterruptsMuxedCodedFrameGroup_3) {
  // Append muxed:
  //        A:[K K K
  //        V:(Nothing yet. This is a jagged start, not simulated.)
  // Remove    *****
  // Append muxed:
  //        A:       K K K K K K]
  //        V:       n n n n K n]
  // Expect:
  //        A:      [K K K K K K]
  //        V [..............K n]
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  std::vector<MuxedStreamInfo> msi(2);
  msi[0] =
      MuxedStreamInfo(kAudioTrackNum, "0K 10K 20K 30K 40K 50K 60K 70K 80D10K");
  msi[1] = MuxedStreamInfo(kVideoTrackNum, "31 41 51 61 71K 81", 10);
  std::unique_ptr<Cluster> cluster = GenerateMuxedCluster(msi);

  // Append the first part of the cluster, up to the beginning of the first
  // video simpleblock. The result should be just 4 audio blocks and no video
  // blocks are appended. Since the stream parser does not yet have a duration
  // for the 4th audio block in this partial cluster append, it is not yet
  // emitted from the parser, and only the first 3 audio blocks are expected to
  // be buffered by and available from the demuxer.
  ASSERT_EQ(kVideoTrackNum, 1);
  int video_start = 0;
  bool found = false;
  while (video_start < cluster->size() - 10) {
    if (cluster->data()[video_start] == 0xA3 &&
        cluster->data()[video_start + 9] == 0x81) {
      found = true;
      break;
    }
    video_start++;
  }

  ASSERT_TRUE(found);
  ASSERT_GT(video_start, 0);
  ASSERT_LT(video_start, cluster->size() - 3);

  ASSERT_TRUE(AppendData(kSourceId, cluster->data(), video_start));
  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [0,30) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ }");

  demuxer_->Remove(kSourceId, base::TimeDelta(),
                   base::TimeDelta::FromMilliseconds(30));

  // Append the remainder of the cluster
  ASSERT_TRUE(AppendData(kSourceId, cluster->data() + video_start,
                         cluster->size() - video_start));

  CheckExpectedRanges(DemuxerStream::AUDIO, "{ [30,90) }");
  CheckExpectedRanges(DemuxerStream::VIDEO, "{ [0,91) }");
  CheckExpectedRanges("{ [30,90) }");
  CheckExpectedBuffers(audio_stream, "30K 40K 50K 60K 70K 80K");
  CheckExpectedBuffers(video_stream, "71K 81");
}

void OnStreamStatusChanged(base::WaitableEvent* event, DemuxerStream* stream,
                           bool enabled, base::TimeDelta) {
  EXPECT_EQ(enabled, stream->enabled());
  event->Signal();
}

void CheckStreamStatusNotifications(DemuxerStream* stream) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  ASSERT_TRUE(stream->enabled());
  stream->SetStreamStatusChangeCB(base::Bind(&OnStreamStatusChanged,
                                             base::Unretained(&event),
                                             base::Unretained(stream)));

  stream->set_enabled(false, base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(event.IsSignaled());

  event.Reset();
  stream->set_enabled(true, base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(event.IsSignaled());
}

TEST_F(ChunkDemuxerTest, StreamStatusNotifications) {
  ASSERT_TRUE(InitDemuxer(HAS_AUDIO | HAS_VIDEO));
  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  EXPECT_NE(NULL, audio_stream);
  CheckStreamStatusNotifications(audio_stream);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  EXPECT_NE(NULL, video_stream);
  CheckStreamStatusNotifications(video_stream);
}

TEST_F(ChunkDemuxerTest, MultipleIds) {
  CreateNewDemuxer();
  EXPECT_CALL(*this, DemuxerOpened());
  EXPECT_CALL(host_, SetDuration(_)).Times(2);
  demuxer_->Initialize(&host_, CreateInitDoneCB(kNoTimestamp, PIPELINE_OK),
                       true);

  const char* kId1 = "id1";
  const char* kId2 = "id2";
  EXPECT_EQ(AddId(kId1, "video/webm", "opus,vp9"), ChunkDemuxer::kOk);
  EXPECT_EQ(AddId(kId2, "video/webm", "opus,vp9"), ChunkDemuxer::kOk);
  scoped_refptr<DecoderBuffer> data1 = ReadTestDataFile("green-a300hz.webm");
  scoped_refptr<DecoderBuffer> data2 = ReadTestDataFile("red-a500hz.webm");

  EXPECT_MEDIA_LOG(FoundStream("audio")).Times(2);
  EXPECT_MEDIA_LOG(FoundStream("video")).Times(2);
  EXPECT_MEDIA_LOG(CodecName("audio", "opus")).Times(2);
  EXPECT_MEDIA_LOG(CodecName("video", "vp9")).Times(2);
  EXPECT_CALL(*this, InitSegmentReceivedMock(_)).Times(2);
  EXPECT_MEDIA_LOG(SegmentMissingFrames("1")).Times(1);

  EXPECT_TRUE(AppendData(kId1, data1->data(), data1->data_size()));
  EXPECT_TRUE(AppendData(kId2, data2->data(), data2->data_size()));
  CheckExpectedRanges(kId1, "{ [0,12007) }");
  CheckExpectedRanges(kId2, "{ [0,10007) }");
}

TEST_F(ChunkDemuxerTest, CompleteInitAfterIdRemoved) {
  CreateNewDemuxer();
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_,
                       CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK), true);

  // Add two ids, then remove one of the ids and verify that adding init segment
  // only for the remaining id still triggers the InitDoneCB.
  const char* kId1 = "id1";
  const char* kId2 = "id2";
  EXPECT_EQ(AddId(kId1, "video/webm", "vp8"), ChunkDemuxer::kOk);
  EXPECT_EQ(AddId(kId2, "video/webm", "vp9"), ChunkDemuxer::kOk);
  demuxer_->RemoveId(kId2);

  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(30));
  EXPECT_MEDIA_LOG(FoundStream("video"));
  EXPECT_MEDIA_LOG(CodecName("video", "vp8"));

  ASSERT_TRUE(AppendInitSegmentWithSourceId(kId1, HAS_VIDEO));
  AppendSingleStreamCluster(kId1, kVideoTrackNum, "0K 30 60 90");
}

TEST_F(ChunkDemuxerTest, RemovingIdMustRemoveStreams) {
  CreateNewDemuxer();
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_,
                       CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK), true);

  const char* kId1 = "id1";
  EXPECT_EQ(AddId(kId1, "video/webm", "vorbis,vp8"), ChunkDemuxer::kOk);

  EXPECT_CALL(*this, InitSegmentReceivedMock(_));
  EXPECT_MEDIA_LOG(FoundStream("video"));
  EXPECT_MEDIA_LOG(CodecName("video", "vp8"));
  EXPECT_MEDIA_LOG(FoundStream("audio"));
  EXPECT_MEDIA_LOG(CodecName("audio", "vorbis"));

  // Append init segment to ensure demuxer streams get created.
  ASSERT_TRUE(AppendInitSegmentWithSourceId(kId1, HAS_AUDIO | HAS_VIDEO));
  EXPECT_NE(NULL, demuxer_->GetStream(DemuxerStream::AUDIO));
  EXPECT_NE(NULL, demuxer_->GetStream(DemuxerStream::VIDEO));

  // Removing the id should remove also the DemuxerStreams.
  demuxer_->RemoveId(kId1);
  EXPECT_EQ(NULL, demuxer_->GetStream(DemuxerStream::AUDIO));
  EXPECT_EQ(NULL, demuxer_->GetStream(DemuxerStream::VIDEO));
}

// TODO(servolk): Add a unit test with multiple audio/video tracks using the
// same codec type in a single SourceBufferState, when WebM parser supports
// multiple tracks. crbug.com/646900

}  // namespace media
