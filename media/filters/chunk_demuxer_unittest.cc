// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/webm/cluster_builder.h"
#include "media/webm/webm_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace media {

static const uint8 kTracksHeader[] = {
  0x16, 0x54, 0xAE, 0x6B,  // Tracks ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // tracks(size = 0)
};

static const int kTracksHeaderSize = sizeof(kTracksHeader);
static const int kTracksSizeOffset = 4;

// The size of TrackEntry element in test file "webm_vp8_track_entry" starts at
// index 1 and spans 8 bytes.
static const int kVideoTrackSizeOffset = 1;
static const int kVideoTrackSizeWidth = 8;
static const int kVideoTrackEntryHeaderSize = kVideoTrackSizeOffset +
                                              kVideoTrackSizeWidth;

static const int kVideoTrackNum = 1;
static const int kAudioTrackNum = 2;

static const int kAudioBlockDuration = 23;
static const int kVideoBlockDuration = 33;

static const char* kSourceId = "SourceId";
static const char* kDefaultFirstClusterRange = "{ [0,46) }";

base::TimeDelta kDefaultDuration() {
  return base::TimeDelta::FromMilliseconds(201224);
}

// Write an integer into buffer in the form of vint that spans 8 bytes.
// The data pointed by |buffer| should be at least 8 bytes long.
// |number| should be in the range 0 <= number < 0x00FFFFFFFFFFFFFF.
static void WriteInt64(uint8* buffer, int64 number) {
  DCHECK(number >= 0 && number < GG_LONGLONG(0x00FFFFFFFFFFFFFF));
  buffer[0] = 0x01;
  int64 tmp = number;
  for (int i = 7; i > 0; i--) {
    buffer[i] = tmp & 0xff;
    tmp >>= 8;
  }
}

MATCHER_P(HasTimestamp, timestamp_in_ms, "") {
  return arg && !arg->IsEndOfStream() &&
      arg->GetTimestamp().InMilliseconds() == timestamp_in_ms;
}

static void OnReadDone(const base::TimeDelta& expected_time,
                       bool* called,
                       const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(expected_time, buffer->GetTimestamp());
  *called = true;
}

static void OnReadDone_EOSExpected(bool* called,
                                   const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_TRUE(buffer->IsEndOfStream());
  *called = true;
}

class MockChunkDemuxerClient : public ChunkDemuxerClient {
 public:
  MockChunkDemuxerClient() {}
  virtual ~MockChunkDemuxerClient() {}

  MOCK_METHOD1(DemuxerOpened, void(ChunkDemuxer* demuxer));
  MOCK_METHOD0(DemuxerClosed, void());
  // TODO(xhwang): This is a workaround of the issue that move-only parameters
  // are not supported in mocked methods. Remove this when the issue is fixed
  // (http://code.google.com/p/googletest/issues/detail?id=395) or when we use
  // std::string instead of scoped_array<uint8> (http://crbug.com/130689).
  MOCK_METHOD2(NeedKeyMock, void(const uint8* init_data, int init_data_size));
  void DemuxerNeedKey(scoped_array<uint8> init_data, int init_data_size) {
    NeedKeyMock(init_data.get(), init_data_size);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChunkDemuxerClient);
};

class ChunkDemuxerTest : public testing::Test {
 protected:
  enum CodecsIndex {
    AUDIO,
    VIDEO,
    MAX_CODECS_INDEX
  };

  // Default cluster to append first for simple tests.
  scoped_ptr<Cluster> kDefaultFirstCluster() {
    return GenerateCluster(0, 4);
  }

  // Default cluster to append after kDefaultFirstCluster()
  // has been appended. This cluster starts with blocks that
  // have timestamps consistent with the end times of the blocks
  // in kDefaultFirstCluster() so that these two clusters represent
  // a continuous region.
  scoped_ptr<Cluster> kDefaultSecondCluster() {
    return GenerateCluster(46, 66, 5);
  }

  ChunkDemuxerTest()
      : client_(new MockChunkDemuxerClient()),
        demuxer_(new ChunkDemuxer(client_.get())) {
  }

  virtual ~ChunkDemuxerTest() {
    ShutdownDemuxer();
  }

  void CreateInitSegment(bool has_audio, bool has_video,
                         bool video_content_encoded,
                         scoped_array<uint8>* buffer,
                         int* size) {
    scoped_refptr<DecoderBuffer> ebml_header;
    scoped_refptr<DecoderBuffer> info;
    scoped_refptr<DecoderBuffer> audio_track_entry;
    scoped_refptr<DecoderBuffer> video_track_entry;
    scoped_refptr<DecoderBuffer> video_content_encodings;

    ebml_header = ReadTestDataFile("webm_ebml_element");

    info = ReadTestDataFile("webm_info_element");

    int tracks_element_size = 0;

    if (has_audio) {
      audio_track_entry = ReadTestDataFile("webm_vorbis_track_entry");
      tracks_element_size += audio_track_entry->GetDataSize();
    }

    if (has_video) {
      video_track_entry = ReadTestDataFile("webm_vp8_track_entry");
      tracks_element_size += video_track_entry->GetDataSize();
      if (video_content_encoded) {
        video_content_encodings = ReadTestDataFile("webm_content_encodings");
        tracks_element_size += video_content_encodings->GetDataSize();
      }
    }

    *size = ebml_header->GetDataSize() + info->GetDataSize() +
        kTracksHeaderSize + tracks_element_size;

    buffer->reset(new uint8[*size]);

    uint8* buf = buffer->get();
    memcpy(buf, ebml_header->GetData(), ebml_header->GetDataSize());
    buf += ebml_header->GetDataSize();

    memcpy(buf, info->GetData(), info->GetDataSize());
    buf += info->GetDataSize();

    memcpy(buf, kTracksHeader, kTracksHeaderSize);
    WriteInt64(buf + kTracksSizeOffset, tracks_element_size);
    buf += kTracksHeaderSize;

    if (has_audio) {
      memcpy(buf, audio_track_entry->GetData(),
             audio_track_entry->GetDataSize());
      buf += audio_track_entry->GetDataSize();
    }

    if (has_video) {
      memcpy(buf, video_track_entry->GetData(),
             video_track_entry->GetDataSize());
      if (video_content_encoded) {
        memcpy(buf + video_track_entry->GetDataSize(),
               video_content_encodings->GetData(),
               video_content_encodings->GetDataSize());
        WriteInt64(buf + kVideoTrackSizeOffset,
                   video_track_entry->GetDataSize() +
                   video_content_encodings->GetDataSize() -
                   kVideoTrackEntryHeaderSize);
        buf += video_content_encodings->GetDataSize();
      }
      buf += video_track_entry->GetDataSize();
    }
  }

  ChunkDemuxer::Status AddId() {
    return AddId(kSourceId, true, true);
  }

  ChunkDemuxer::Status AddId(const std::string& source_id,
                             bool has_audio, bool has_video) {
    std::vector<std::string> codecs;
    std::string type;

    if (has_audio) {
      codecs.push_back("vorbis");
      type = "audio/webm";
    }

    if (has_video) {
      codecs.push_back("vp8");
      type = "video/webm";
    }

    if (!has_audio && !has_video) {
      return AddId(kSourceId, true, true);
    }

    return demuxer_->AddId(source_id, type, codecs);
  }

  bool AppendData(const uint8* data, size_t length) {
    return AppendData(kSourceId, data, length);
  }

  bool AppendData(const std::string& source_id,
                  const uint8* data, size_t length) {
    CHECK(length);
    EXPECT_CALL(host_, AddBufferedTimeRange(_, _)).Times(AnyNumber())
        .WillRepeatedly(SaveArg<1>(&buffered_time_));
    return demuxer_->AppendData(source_id, data, length);
  }

  bool AppendDataInPieces(const uint8* data, size_t length) {
    return AppendDataInPieces(data, length, 7);
  }

  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      base::TimeDelta old_buffered_time = buffered_time_;
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;

      EXPECT_GE(buffered_time_, old_buffered_time);
    }
    return true;
  }

  bool AppendInitSegment(bool has_audio, bool has_video,
                         bool video_content_encoded) {
    return AppendInitSegment(kSourceId, has_audio, has_video,
                             video_content_encoded);
  }

  bool AppendInitSegment(const std::string& source_id,
                         bool has_audio, bool has_video,
                         bool video_content_encoded) {
    scoped_array<uint8> info_tracks;
    int info_tracks_size = 0;
    CreateInitSegment(has_audio, has_video, video_content_encoded,
                      &info_tracks, &info_tracks_size);
    return AppendData(source_id, info_tracks.get(), info_tracks_size);
  }

  void InitDoneCalled(PipelineStatus expected_status,
                      PipelineStatus status) {
    EXPECT_EQ(status, expected_status);
  }

  PipelineStatusCB CreateInitDoneCB(const base::TimeDelta& expected_duration,
                                    PipelineStatus expected_status) {
    if (expected_status == PIPELINE_OK)
      EXPECT_CALL(host_, SetDuration(expected_duration));
    return CreateInitDoneCB(expected_status);
  }

  PipelineStatusCB CreateInitDoneCB(PipelineStatus expected_status) {
    return base::Bind(&ChunkDemuxerTest::InitDoneCalled,
                      base::Unretained(this),
                      expected_status);
  }

  bool InitDemuxer(bool has_audio, bool has_video,
                   bool video_content_encoded) {
    PipelineStatus expected_status =
        (has_audio || has_video) ? PIPELINE_OK : DEMUXER_ERROR_COULD_NOT_OPEN;

    EXPECT_CALL(*client_, DemuxerOpened(_));
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(kDefaultDuration(), expected_status));

    if (AddId(kSourceId, has_audio, has_video) != ChunkDemuxer::kOk)
      return false;

    return AppendInitSegment(has_audio, has_video, video_content_encoded);
  }

  bool InitDemuxerAudioAndVideoSources(const std::string& audio_id,
                                       const std::string& video_id) {
    EXPECT_CALL(*client_, DemuxerOpened(_));
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

    if (AddId(audio_id, true, false) != ChunkDemuxer::kOk)
      return false;
    if (AddId(video_id, false, true) != ChunkDemuxer::kOk)
      return false;

    bool success = AppendInitSegment(audio_id, true, false, false);
    success &= AppendInitSegment(video_id, false, true, false);
    return success;
  }

  void ShutdownDemuxer() {
    if (demuxer_) {
      EXPECT_CALL(*client_, DemuxerClosed());
      demuxer_->Shutdown();
    }
  }

  void AddSimpleBlock(ClusterBuilder* cb, int track_num, int64 timecode) {
    uint8 data[] = { 0x00 };
    cb->AddSimpleBlock(track_num, timecode, 0, data, sizeof(data));
  }

  scoped_ptr<Cluster> GenerateCluster(int timecode, int block_count) {
    return GenerateCluster(timecode, timecode, block_count);
  }

  scoped_ptr<Cluster> GenerateCluster(int first_audio_timecode,
                                      int first_video_timecode,
                                      int block_count) {
    CHECK_GT(block_count, 0);

    int size = 10;
    scoped_array<uint8> data(new uint8[size]);

    ClusterBuilder cb;
    cb.SetClusterTimecode(std::min(first_audio_timecode, first_video_timecode));

    if (block_count == 1) {
      cb.AddBlockGroup(kAudioTrackNum, first_audio_timecode,
                       kAudioBlockDuration, kWebMFlagKeyframe,
                       data.get(), size);
      return cb.Finish();
    }

    int audio_timecode = first_audio_timecode;
    int video_timecode = first_video_timecode;

    // Create simple blocks for everything except the last 2 blocks.
    // The first video frame must be a keyframe.
    uint8 video_flag = kWebMFlagKeyframe;
    for (int i = 0; i < block_count - 2; i++) {
      if (audio_timecode <= video_timecode) {
        cb.AddSimpleBlock(kAudioTrackNum, audio_timecode, kWebMFlagKeyframe,
                          data.get(), size);
        audio_timecode += kAudioBlockDuration;
        continue;
      }

      cb.AddSimpleBlock(kVideoTrackNum, video_timecode, video_flag, data.get(),
                        size);
      video_timecode += kVideoBlockDuration;
      video_flag = 0;
    }

    // Make the last 2 blocks BlockGroups so that they don't get delayed by the
    // block duration calculation logic.
    if (audio_timecode <= video_timecode) {
      cb.AddBlockGroup(kAudioTrackNum, audio_timecode, kAudioBlockDuration,
                       kWebMFlagKeyframe, data.get(), size);
      cb.AddBlockGroup(kVideoTrackNum, video_timecode, kVideoBlockDuration,
                       video_flag, data.get(), size);
    } else {
      cb.AddBlockGroup(kVideoTrackNum, video_timecode, kVideoBlockDuration,
                       video_flag, data.get(), size);
      cb.AddBlockGroup(kAudioTrackNum, audio_timecode, kAudioBlockDuration,
                       kWebMFlagKeyframe, data.get(), size);
    }

    return cb.Finish();
  }

  scoped_ptr<Cluster> GenerateSingleStreamCluster(int timecode,
                                                  int end_timecode,
                                                  int track_number,
                                                  int block_duration) {
    CHECK_GT(end_timecode, timecode);

    int size = 10;
    scoped_array<uint8> data(new uint8[size]);

    ClusterBuilder cb;
    cb.SetClusterTimecode(timecode);

    // Create simple blocks for everything except the last block.
    for (int i = 0; timecode < (end_timecode - block_duration); i++) {
      cb.AddSimpleBlock(track_number, timecode, kWebMFlagKeyframe,
                        data.get(), size);
      timecode += block_duration;
    }

    // Make the last block a BlockGroup so that it doesn't get delayed by the
    // block duration calculation logic.
    cb.AddBlockGroup(track_number, timecode, block_duration,
                      kWebMFlagKeyframe, data.get(), size);
    return cb.Finish();
  }

  void GenerateExpectedReads(int timecode, int block_count,
                             DemuxerStream* audio,
                             DemuxerStream* video) {
    GenerateExpectedReads(timecode, timecode, block_count, audio, video);
  }

  void GenerateExpectedReads(int start_audio_timecode,
                             int start_video_timecode,
                             int block_count, DemuxerStream* audio,
                             DemuxerStream* video) {
    CHECK_GT(block_count, 0);

    if (block_count == 1) {
      ExpectRead(audio, start_audio_timecode);
      return;
    }

    int audio_timecode = start_audio_timecode;
    int video_timecode = start_video_timecode;

    for (int i = 0; i < block_count; i++) {
      if (audio_timecode <= video_timecode) {
        ExpectRead(audio, audio_timecode);
        audio_timecode += kAudioBlockDuration;
        continue;
      }

      ExpectRead(video, video_timecode);
      video_timecode += kVideoBlockDuration;
    }
  }

  void GenerateExpectedReads(int timecode, int block_count,
                             DemuxerStream* stream, int block_duration) {
    CHECK_GT(block_count, 0);
    int stream_timecode = timecode;

    for (int i = 0; i < block_count; i++) {
      ExpectRead(stream, stream_timecode);
      stream_timecode += block_duration;
    }
  }

  void CheckExpectedRanges(const std::string& expected) {
    CheckExpectedRanges(kSourceId, expected);
  }

  void CheckExpectedRanges(const std::string&  id,
                           const std::string& expected) {
    Ranges<base::TimeDelta> r = demuxer_->GetBufferedRanges(id);

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      ss << "[" << r.start(i).InMilliseconds() << ","
         << r.end(i).InMilliseconds() << ") ";
    }
    ss << "}";
    EXPECT_EQ(ss.str(), expected);
  }

  MOCK_METHOD1(ReadDone, void(const scoped_refptr<DecoderBuffer>&));

  void ExpectRead(DemuxerStream* stream, int64 timestamp_in_ms) {
    EXPECT_CALL(*this, ReadDone(HasTimestamp(timestamp_in_ms)));
    stream->Read(base::Bind(&ChunkDemuxerTest::ReadDone,
                            base::Unretained(this)));
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
    return ParseWebMFile(filename, timestamps, duration, true, true);
  }

  bool ParseWebMFile(const std::string& filename,
                     const BufferTimestamps* timestamps,
                     const base::TimeDelta& duration,
                     bool has_audio, bool has_video) {
    EXPECT_CALL(*client_, DemuxerOpened(_));
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(duration, PIPELINE_OK));

    if (AddId(kSourceId, has_audio, has_video) != ChunkDemuxer::kOk)
      return false;

    // Read a WebM file into memory and send the data to the demuxer.
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    if (!AppendDataInPieces(buffer->GetData(), buffer->GetDataSize(), 512))
      return false;

    scoped_refptr<DemuxerStream> audio =
        demuxer_->GetStream(DemuxerStream::AUDIO);
    scoped_refptr<DemuxerStream> video =
        demuxer_->GetStream(DemuxerStream::VIDEO);

    // Verify that the timestamps on the first few packets match what we
    // expect.
    for (size_t i = 0;
         (timestamps[i].audio_time_ms != kSkip ||
          timestamps[i].video_time_ms != kSkip);
         i++) {
      bool audio_read_done = false;
      bool video_read_done = false;

      if (timestamps[i].audio_time_ms != kSkip) {
        DCHECK(audio);
        audio->Read(base::Bind(&OnReadDone,
                               base::TimeDelta::FromMilliseconds(
                                   timestamps[i].audio_time_ms),
                               &audio_read_done));
        EXPECT_TRUE(audio_read_done);
      }

      if (timestamps[i].video_time_ms != kSkip) {
        DCHECK(video);
        video->Read(base::Bind(&OnReadDone,
                               base::TimeDelta::FromMilliseconds(
                                   timestamps[i].video_time_ms),
                               &video_read_done));

        EXPECT_TRUE(video_read_done);
      }
    }

    return true;
  }

  MockDemuxerHost host_;
  base::TimeDelta buffered_time_;

  scoped_ptr<MockChunkDemuxerClient> client_;
  scoped_refptr<ChunkDemuxer> demuxer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerTest);
};

TEST_F(ChunkDemuxerTest, TestInit) {
  // Test no streams, audio-only, video-only, and audio & video scenarios,
  // with video content encoded or not.
  for (int i = 0; i < 8; i++) {
    bool has_audio = (i & 0x1) != 0;
    bool has_video = (i & 0x2) != 0;
    bool video_content_encoded = (i & 0x4) != 0;

    // No test on invalid combination.
    if (!has_video && video_content_encoded)
      continue;

    client_.reset(new MockChunkDemuxerClient());
    demuxer_ = new ChunkDemuxer(client_.get());
    if (has_video && video_content_encoded)
      EXPECT_CALL(*client_, NeedKeyMock(NotNull(), 16));

    ASSERT_TRUE(InitDemuxer(has_audio, has_video, video_content_encoded));

    scoped_refptr<DemuxerStream> audio_stream =
        demuxer_->GetStream(DemuxerStream::AUDIO);
    if (has_audio) {
      ASSERT_TRUE(audio_stream);

      const AudioDecoderConfig& config = audio_stream->audio_decoder_config();
      EXPECT_EQ(kCodecVorbis, config.codec());
      EXPECT_EQ(16, config.bits_per_channel());
      EXPECT_EQ(CHANNEL_LAYOUT_STEREO, config.channel_layout());
      EXPECT_EQ(44100, config.samples_per_second());
      EXPECT_TRUE(config.extra_data());
      EXPECT_GT(config.extra_data_size(), 0u);
    } else {
      EXPECT_FALSE(audio_stream);
    }

    scoped_refptr<DemuxerStream> video_stream =
        demuxer_->GetStream(DemuxerStream::VIDEO);
    if (has_video) {
      EXPECT_TRUE(video_stream);
    } else {
      EXPECT_FALSE(video_stream);
    }

    ShutdownDemuxer();
    demuxer_ = NULL;
  }
}

// Makes sure that Seek() reports an error if Shutdown()
// is called before the first cluster is passed to the demuxer.
TEST_F(ChunkDemuxerTest, TestShutdownBeforeFirstSeekCompletes) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_ERROR_ABORT));
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, TestAppendDataAfterSeek) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  InSequence s;

  EXPECT_CALL(*this, Checkpoint(1));

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_OK));

  EXPECT_CALL(*this, Checkpoint(2));

  scoped_ptr<Cluster> cluster(kDefaultFirstCluster());

  Checkpoint(1);

  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));

  Checkpoint(2);
}

// Test the case where a Seek() is requested while the parser
// is in the middle of cluster. This is to verify that the parser
// does not reset itself on a seek.
TEST_F(ChunkDemuxerTest, TestSeekWhileParsingCluster) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  InSequence s;

  scoped_ptr<Cluster> cluster_a(GenerateCluster(0, 6));

  // Split the cluster into two appends at an arbitrary point near the end.
  int first_append_size = cluster_a->size() - 11;
  int second_append_size = cluster_a->size() - first_append_size;

  // Append the first part of the cluster.
  ASSERT_TRUE(AppendData(cluster_a->data(), first_append_size));

  ExpectRead(audio, 0);
  ExpectRead(video, 0);
  ExpectRead(audio, kAudioBlockDuration);
  // Note: We skip trying to read a video buffer here because computing
  // the duration for this block relies on successfully parsing the last block
  // in the cluster the cluster.
  ExpectRead(audio, 2 * kAudioBlockDuration);

  demuxer_->StartWaitingForSeek();
  demuxer_->Seek(base::TimeDelta::FromSeconds(5),
                 NewExpectedStatusCB(PIPELINE_OK));

  // Append the rest of the cluster.
  ASSERT_TRUE(AppendData(cluster_a->data() + first_append_size,
                         second_append_size));

  // Append the new cluster and verify that only the blocks
  // in the new cluster are returned.
  scoped_ptr<Cluster> cluster_b(GenerateCluster(5000, 6));
  ASSERT_TRUE(AppendData(cluster_b->data(), cluster_b->size()));
  GenerateExpectedReads(5000, 6, audio, video);
}

// Test the case where AppendData() is called before Init().
TEST_F(ChunkDemuxerTest, TestAppendDataBeforeInit) {
  scoped_array<uint8> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(true, true, false, &info_tracks, &info_tracks_size);

  EXPECT_FALSE(demuxer_->AppendData(kSourceId, info_tracks.get(),
                                    info_tracks_size));
}

// Make sure Read() callbacks are dispatched with the proper data.
TEST_F(ChunkDemuxerTest, TestRead) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done = false;
  bool video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &video_read_done));

  scoped_ptr<Cluster> cluster(kDefaultFirstCluster());

  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, TestOutOfOrderClusters) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_ptr<Cluster> cluster_a(GenerateCluster(10, 4));

  ASSERT_TRUE(AppendData(cluster_a->data(), cluster_a->size()));

  // Cluster B starts before cluster_a and has data
  // that overlaps.
  scoped_ptr<Cluster> cluster_b(GenerateCluster(5, 4));

  // Make sure that AppendData() does not fail.
  ASSERT_TRUE(AppendData(cluster_b->data(), cluster_b->size()));

  // Verify that AppendData() can still accept more data.
  scoped_ptr<Cluster> cluster_c(GenerateCluster(45, 2));
  ASSERT_TRUE(demuxer_->AppendData(kSourceId, cluster_c->data(),
                                   cluster_c->size()));
}

TEST_F(ChunkDemuxerTest, TestNonMonotonicButAboveClusterTimecode) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  ClusterBuilder cb;

  // Test the case where block timecodes are not monotonically
  // increasing but stay above the cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 7);
  AddSimpleBlock(&cb, kVideoTrackNum, 15);
  scoped_ptr<Cluster> cluster_a(cb.Finish());

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  ASSERT_TRUE(AppendData(cluster_a->data(), cluster_a->size()));

  // Verify that AppendData() doesn't accept more data now.
  scoped_ptr<Cluster> cluster_b(GenerateCluster(20, 2));
  EXPECT_FALSE(demuxer_->AppendData(kSourceId, cluster_b->data(),
                                    cluster_b->size()));
}

TEST_F(ChunkDemuxerTest, TestBackwardsAndBeforeClusterTimecode) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  ClusterBuilder cb;

  // Test timecodes going backwards and including values less than the cluster
  // timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 3);
  AddSimpleBlock(&cb, kVideoTrackNum, 3);
  scoped_ptr<Cluster> cluster_a(cb.Finish());

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  ASSERT_TRUE(AppendData(cluster_a->data(), cluster_a->size()));

  // Verify that AppendData() doesn't accept more data now.
  scoped_ptr<Cluster> cluster_b(GenerateCluster(6, 2));
  EXPECT_FALSE(demuxer_->AppendData(kSourceId, cluster_b->data(),
                                    cluster_b->size()));
}


TEST_F(ChunkDemuxerTest, TestPerStreamMonotonicallyIncreasingTimestamps) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  ClusterBuilder cb;

  // Test monotonic increasing timestamps on a per stream
  // basis.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 4);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> cluster(cb.Finish());

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));
}

TEST_F(ChunkDemuxerTest, TestMonotonicallyIncreasingTimestampsAcrossClusters) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  ClusterBuilder cb;

  // Test monotonic increasing timestamps on a per stream
  // basis across clusters.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  scoped_ptr<Cluster> cluster_a(cb.Finish());

  ASSERT_TRUE(AppendData(cluster_a->data(), cluster_a->size()));

  cb.SetClusterTimecode(4);
  AddSimpleBlock(&cb, kAudioTrackNum, 4);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> cluster_b(cb.Finish());

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  ASSERT_TRUE(AppendData(cluster_b->data(), cluster_b->size()));

  // Verify that AppendData() doesn't accept more data now.
  scoped_ptr<Cluster> cluster_c(GenerateCluster(10, 2));
  EXPECT_FALSE(demuxer_->AppendData(kSourceId, cluster_c->data(),
                                    cluster_c->size()));
}

// Test the case where a cluster is passed to AppendData() before
// INFO & TRACKS data.
TEST_F(ChunkDemuxerTest, TestClusterBeforeInitSegment) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  scoped_ptr<Cluster> cluster(GenerateCluster(0, 1));

  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));
}

// Test cases where we get an EndOfStream() call during initialization.
TEST_F(ChunkDemuxerTest, TestEOSDuringInit) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));
  demuxer_->EndOfStream(PIPELINE_OK);
}

TEST_F(ChunkDemuxerTest, TestEndOfStreamWithNoAppend) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  CheckExpectedRanges("{ }");
  demuxer_->EndOfStream(PIPELINE_OK);
  CheckExpectedRanges("{ }");
}

TEST_F(ChunkDemuxerTest, TestDecodeErrorEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_ptr<Cluster> cluster(kDefaultFirstCluster());
  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));
  CheckExpectedRanges(kDefaultFirstClusterRange);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  demuxer_->EndOfStream(PIPELINE_ERROR_DECODE);
  CheckExpectedRanges(kDefaultFirstClusterRange);
}

TEST_F(ChunkDemuxerTest, TestNetworkErrorEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_ptr<Cluster> cluster(kDefaultFirstCluster());
  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));
  CheckExpectedRanges(kDefaultFirstClusterRange);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_NETWORK));
  demuxer_->EndOfStream(PIPELINE_ERROR_NETWORK);
}

// Helper class to reduce duplicate code when testing end of stream
// Read() behavior.
class EndOfStreamHelper {
 public:
  explicit EndOfStreamHelper(const scoped_refptr<Demuxer> demuxer)
      : demuxer_(demuxer),
        audio_read_done_(false),
        video_read_done_(false) {
  }

  // Request a read on the audio and video streams.
  void RequestReads() {
    EXPECT_FALSE(audio_read_done_);
    EXPECT_FALSE(video_read_done_);

    scoped_refptr<DemuxerStream> audio =
        demuxer_->GetStream(DemuxerStream::AUDIO);
    scoped_refptr<DemuxerStream> video =
        demuxer_->GetStream(DemuxerStream::VIDEO);

    audio->Read(base::Bind(&OnEndOfStreamReadDone,
                           &audio_read_done_));

    video->Read(base::Bind(&OnEndOfStreamReadDone,
                           &video_read_done_));
  }

  // Check to see if |audio_read_done_| and |video_read_done_| variables
  // match |expected|.
  void CheckIfReadDonesWereCalled(bool expected) {
    EXPECT_EQ(expected, audio_read_done_);
    EXPECT_EQ(expected, video_read_done_);
  }

 private:
  static void OnEndOfStreamReadDone(
      bool* called, const scoped_refptr<DecoderBuffer>& buffer) {
    EXPECT_TRUE(buffer->IsEndOfStream());
    *called = true;
  }

  scoped_refptr<Demuxer> demuxer_;
  bool audio_read_done_;
  bool video_read_done_;

  DISALLOW_COPY_AND_ASSIGN(EndOfStreamHelper);
};

// Make sure that all pending reads that we don't have media data for get an
// "end of stream" buffer when EndOfStream() is called.
TEST_F(ChunkDemuxerTest, TestEndOfStreamWithPendingReads) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_);
  EndOfStreamHelper end_of_stream_helper_2(demuxer_);

  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &audio_read_done_1));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &video_read_done_1));

  end_of_stream_helper_1.RequestReads();
  end_of_stream_helper_2.RequestReads();

  scoped_ptr<Cluster> cluster(GenerateCluster(0, 2));

  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);
  end_of_stream_helper_1.CheckIfReadDonesWereCalled(false);
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(false);

  demuxer_->EndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);
}

// Make sure that all Read() calls after we get an EndOfStream()
// call return an "end of stream" buffer.
TEST_F(ChunkDemuxerTest, TestReadsAfterEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_);
  EndOfStreamHelper end_of_stream_helper_2(demuxer_);
  EndOfStreamHelper end_of_stream_helper_3(demuxer_);

  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &audio_read_done_1));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &video_read_done_1));

  end_of_stream_helper_1.RequestReads();

  scoped_ptr<Cluster> cluster(GenerateCluster(0, 2));

  ASSERT_TRUE(AppendData(cluster->data(), cluster->size()));

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);
  end_of_stream_helper_1.CheckIfReadDonesWereCalled(false);

  EXPECT_TRUE(demuxer_->EndOfStream(PIPELINE_OK));

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);

  // Request a few more reads and make sure we immediately get
  // end of stream buffers.
  end_of_stream_helper_2.RequestReads();
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);

  end_of_stream_helper_3.RequestReads();
  end_of_stream_helper_3.CheckIfReadDonesWereCalled(true);
}

// Make sure AppendData() will accept elements that span multiple calls.
TEST_F(ChunkDemuxerTest, TestAppendingInPieces) {

  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  scoped_array<uint8> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(true, true, false, &info_tracks, &info_tracks_size);

  scoped_ptr<Cluster> cluster_a(kDefaultFirstCluster());
  scoped_ptr<Cluster> cluster_b(kDefaultSecondCluster());

  size_t buffer_size = info_tracks_size + cluster_a->size() + cluster_b->size();
  scoped_array<uint8> buffer(new uint8[buffer_size]);
  uint8* dst = buffer.get();
  memcpy(dst, info_tracks.get(), info_tracks_size);
  dst += info_tracks_size;

  memcpy(dst, cluster_a->data(), cluster_a->size());
  dst += cluster_a->size();

  memcpy(dst, cluster_b->data(), cluster_b->size());
  dst += cluster_b->size();

  ASSERT_TRUE(AppendDataInPieces(buffer.get(), buffer_size));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  ASSERT_TRUE(audio);
  ASSERT_TRUE(video);

  GenerateExpectedReads(0, 9, audio, video);
}

TEST_F(ChunkDemuxerTest, TestWebMFile_AudioAndVideo) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, 0},
    {33, 3},
    {67, 6},
    {100, 9},
    {133, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2744)));
}

TEST_F(ChunkDemuxerTest, TestWebMFile_LiveAudioAndVideo) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, 0},
    {33, 3},
    {67, 6},
    {100, 9},
    {133, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-live.webm", buffer_timestamps,
                            kInfiniteDuration()));
}

TEST_F(ChunkDemuxerTest, TestWebMFile_AudioOnly) {
  struct BufferTimestamps buffer_timestamps[] = {
    {kSkip, 0},
    {kSkip, 3},
    {kSkip, 6},
    {kSkip, 9},
    {kSkip, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-audio-only.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2744),
                            true, false));
}

TEST_F(ChunkDemuxerTest, TestWebMFile_VideoOnly) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, kSkip},
    {33, kSkip},
    {67, kSkip},
    {100, kSkip},
    {133, kSkip},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-video-only.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2703),
                            false, true));
}

TEST_F(ChunkDemuxerTest, TestWebMFile_AltRefFrames) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, 0},
    {33, 3},
    {33, 6},
    {67, 9},
    {100, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-altref.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2767)));
}

// Verify that we output buffers before the entire cluster has been parsed.
TEST_F(ChunkDemuxerTest, TestIncrementalClusterParsing) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_ptr<Cluster> cluster(GenerateCluster(0, 6));
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done = false;
  bool video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &video_read_done));

  // Make sure the reads haven't completed yet.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Append data one byte at a time until the audio read completes.
  int i = 0;
  for (; i < cluster->size() && !audio_read_done; ++i) {
    ASSERT_TRUE(AppendData(cluster->data() + i, 1));
  }

  EXPECT_TRUE(audio_read_done);
  EXPECT_FALSE(video_read_done);
  EXPECT_GT(i, 0);
  EXPECT_LT(i, cluster->size());

  // Append data one byte at a time until the video read completes.
  for (; i < cluster->size() && !video_read_done; ++i) {
    ASSERT_TRUE(AppendData(cluster->data() + i, 1));
  }

  EXPECT_TRUE(video_read_done);
  EXPECT_LT(i, cluster->size());

  audio_read_done = false;
  video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(23),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(33),
                         &video_read_done));

  // Make sure the reads haven't completed yet.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Append the remaining data.
  ASSERT_LT(i, cluster->size());
  ASSERT_TRUE(AppendData(cluster->data() + i, cluster->size() - i));

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}


TEST_F(ChunkDemuxerTest, TestParseErrorDuringInit) {
  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));

  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  ASSERT_TRUE(AppendInitSegment(true, true, false));

  uint8 tmp = 0;
  ASSERT_TRUE(demuxer_->AppendData(kSourceId, &tmp, 1));
}

TEST_F(ChunkDemuxerTest, TestAVHeadersWithAudioOnlyType) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(),
                               DEMUXER_ERROR_COULD_NOT_OPEN));

  std::vector<std::string> codecs(1);
  codecs[0] = "vorbis";
  ASSERT_EQ(demuxer_->AddId(kSourceId, "audio/webm", codecs),
            ChunkDemuxer::kOk);

  ASSERT_TRUE(AppendInitSegment(true, true, false));
}

TEST_F(ChunkDemuxerTest, TestAVHeadersWithVideoOnlyType) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(),
                               DEMUXER_ERROR_COULD_NOT_OPEN));

  std::vector<std::string> codecs(1);
  codecs[0] = "vp8";
  ASSERT_EQ(demuxer_->AddId(kSourceId, "video/webm", codecs),
            ChunkDemuxer::kOk);

  ASSERT_TRUE(AppendInitSegment(true, true, false));
}

TEST_F(ChunkDemuxerTest, TestMultipleHeaders) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  scoped_ptr<Cluster> cluster_a(kDefaultFirstCluster());
  ASSERT_TRUE(AppendData(cluster_a->data(), cluster_a->size()));

  // Append another identical initialization segment.
  ASSERT_TRUE(AppendInitSegment(true, true, false));

  scoped_ptr<Cluster> cluster_b(kDefaultSecondCluster());
  ASSERT_TRUE(AppendData(cluster_b->data(), cluster_b->size()));

  GenerateExpectedReads(0, 9, audio, video);
}

TEST_F(ChunkDemuxerTest, TestAddSeparateSourcesForAudioAndVideo) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  scoped_ptr<Cluster> cluster_a(
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));

  scoped_ptr<Cluster> cluster_v(
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));

  // Append audio and video data into separate source ids.
  ASSERT_TRUE(AppendData(audio_id, cluster_a->data(), cluster_a->size()));
  GenerateExpectedReads(0, 4, audio, kAudioBlockDuration);
  ASSERT_TRUE(AppendData(video_id, cluster_v->data(), cluster_v->size()));
  GenerateExpectedReads(0, 4, video, kVideoBlockDuration);
}

TEST_F(ChunkDemuxerTest, TestAddIdFailures) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  std::string audio_id = "audio1";
  std::string video_id = "video1";

  ASSERT_EQ(AddId(audio_id, true, false), ChunkDemuxer::kOk);

  // Adding an id with audio/video should fail because we already added audio.
  ASSERT_EQ(AddId(), ChunkDemuxer::kReachedIdLimit);

  ASSERT_TRUE(AppendInitSegment(audio_id, true, false, false));

  // Adding an id after append should fail.
  ASSERT_EQ(AddId(video_id, false, true), ChunkDemuxer::kReachedIdLimit);
}

// Test that Read() calls after a RemoveId() return "end of stream" buffers.
TEST_F(ChunkDemuxerTest, TestRemoveId) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  scoped_ptr<Cluster> cluster_a(
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));

  scoped_ptr<Cluster> cluster_v(
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));

  // Append audio and video data into separate source ids.
  ASSERT_TRUE(AppendData(audio_id, cluster_a->data(), cluster_a->size()));
  ASSERT_TRUE(AppendData(video_id, cluster_v->data(), cluster_v->size()));

  // Read() from audio should return normal buffers.
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  GenerateExpectedReads(0, 4, audio, kAudioBlockDuration);

  // Remove the audio id.
  demuxer_->RemoveId(audio_id);

  // Read() from audio should return "end of stream" buffers.
  bool audio_read_done = false;
  audio->Read(base::Bind(&OnReadDone_EOSExpected,
                         &audio_read_done));
  EXPECT_TRUE(audio_read_done);

  // Read() from video should still return normal buffers.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  GenerateExpectedReads(0, 4, video, kVideoBlockDuration);
}

// Test that Seek() successfully seeks to all source IDs.
TEST_F(ChunkDemuxerTest, TestSeekAudioAndVideoSources) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  scoped_ptr<Cluster> cluster_a1(
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));

  scoped_ptr<Cluster> cluster_v1(
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));

  ASSERT_TRUE(AppendData(audio_id, cluster_a1->data(), cluster_a1->size()));
  ASSERT_TRUE(AppendData(video_id, cluster_v1->data(), cluster_v1->size()));

  // Read() should return buffers at 0.
  bool audio_read_done = false;
  bool video_read_done = false;
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &audio_read_done));
  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(0),
                         &video_read_done));
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // Seek to 3 (an unbuffered region).
  demuxer_->StartWaitingForSeek();
  demuxer_->Seek(base::TimeDelta::FromSeconds(3),
                 NewExpectedStatusCB(PIPELINE_OK));

  audio_read_done = false;
  video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromSeconds(3),
                         &audio_read_done));
  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromSeconds(3),
                         &video_read_done));

  // Read()s should not return until after data is appended at the Seek point.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  scoped_ptr<Cluster> cluster_a2(
      GenerateSingleStreamCluster(3000, 3092, kAudioTrackNum,
                                  kAudioBlockDuration));

  scoped_ptr<Cluster> cluster_v2(
      GenerateSingleStreamCluster(3000, 3132, kVideoTrackNum,
                                  kVideoBlockDuration));

  ASSERT_TRUE(AppendData(audio_id, cluster_a2->data(), cluster_a2->size()));
  ASSERT_TRUE(AppendData(video_id, cluster_v2->data(), cluster_v2->size()));

  // Read() should return buffers at 3.
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

// Test ranges in an audio-only stream.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioIdOnly) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(kSourceId, true, false), ChunkDemuxer::kOk);
  ASSERT_TRUE(AppendInitSegment(true, false, false));

  // Test a simple cluster.
  scoped_ptr<Cluster> cluster_1(GenerateSingleStreamCluster(0, 92,
                                kAudioTrackNum, kAudioBlockDuration));
  ASSERT_TRUE(AppendData(cluster_1->data(), cluster_1->size()));

  CheckExpectedRanges("{ [0,92) }");

  // Append a disjoint cluster to check for two separate ranges.
  scoped_ptr<Cluster> cluster_2(GenerateSingleStreamCluster(150, 219,
                                kAudioTrackNum, kAudioBlockDuration));

  ASSERT_TRUE(AppendData(cluster_2->data(), cluster_2->size()));

  CheckExpectedRanges("{ [0,92) [150,219) }");
}

// Test ranges in a video-only stream.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_VideoIdOnly) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(kSourceId, false, true), ChunkDemuxer::kOk);
  ASSERT_TRUE(AppendInitSegment(false, true, false));

  // Test a simple cluster.
  scoped_ptr<Cluster> cluster_1(GenerateSingleStreamCluster(0, 132,
                                kVideoTrackNum, kVideoBlockDuration));

  ASSERT_TRUE(AppendData(cluster_1->data(), cluster_1->size()));

  CheckExpectedRanges("{ [0,132) }");

  // Append a disjoint cluster to check for two separate ranges.
  scoped_ptr<Cluster> cluster_2(GenerateSingleStreamCluster(150, 249,
                                kVideoTrackNum, kVideoBlockDuration));

  ASSERT_TRUE(AppendData(cluster_2->data(), cluster_2->size()));

  CheckExpectedRanges("{ [0,132) [150,249) }");
}

TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioVideo) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  // Audio: 0 -> 23
  // Video: 0 -> 33
  // Buffered Range: 0 -> 23
  // Audio block duration is smaller than video block duration,
  // so the buffered ranges should correspond to the audio blocks.
  scoped_ptr<Cluster> cluster_a0(
      GenerateSingleStreamCluster(0, kAudioBlockDuration, kAudioTrackNum,
                                  kAudioBlockDuration));

  scoped_ptr<Cluster> cluster_v0(
      GenerateSingleStreamCluster(0, kVideoBlockDuration, kVideoTrackNum,
                                  kVideoBlockDuration));

  ASSERT_TRUE(AppendData(cluster_a0->data(), cluster_a0->size()));
  ASSERT_TRUE(AppendData(cluster_v0->data(), cluster_v0->size()));

  CheckExpectedRanges("{ [0,23) }");

  // Audio: 100 -> 150
  // Video: 120 -> 170
  // Buffered Range: 120 -> 150  (end overlap)
  scoped_ptr<Cluster> cluster_a1(
      GenerateSingleStreamCluster(100, 150, kAudioTrackNum, 50));

  scoped_ptr<Cluster> cluster_v1(
      GenerateSingleStreamCluster(120, 170, kVideoTrackNum, 50));

  ASSERT_TRUE(AppendData(cluster_a1->data(), cluster_a1->size()));
  ASSERT_TRUE(AppendData(cluster_v1->data(), cluster_v1->size()));

  CheckExpectedRanges("{ [0,23) [120,150) }");

  // Audio: 220 -> 290
  // Video: 200 -> 270
  // Buffered Range: 220 -> 270  (front overlap)
  scoped_ptr<Cluster> cluster_a2(
      GenerateSingleStreamCluster(220, 290, kAudioTrackNum, 70));

  scoped_ptr<Cluster> cluster_v2(
      GenerateSingleStreamCluster(200, 270, kVideoTrackNum, 70));

  ASSERT_TRUE(AppendData(cluster_a2->data(), cluster_a2->size()));
  ASSERT_TRUE(AppendData(cluster_v2->data(), cluster_v2->size()));

  CheckExpectedRanges("{ [0,23) [120,150) [220,270) }");

  // Audio: 320 -> 350
  // Video: 300 -> 370
  // Buffered Range: 320 -> 350  (complete overlap, audio)
  scoped_ptr<Cluster> cluster_a3(
      GenerateSingleStreamCluster(320, 350, kAudioTrackNum, 30));

  scoped_ptr<Cluster> cluster_v3(
      GenerateSingleStreamCluster(300, 370, kVideoTrackNum, 70));

  ASSERT_TRUE(AppendData(cluster_a3->data(), cluster_a3->size()));
  ASSERT_TRUE(AppendData(cluster_v3->data(), cluster_v3->size()));

  CheckExpectedRanges("{ [0,23) [120,150) [220,270) [320,350) }");

  // Audio: 400 -> 470
  // Video: 420 -> 450
  // Buffered Range: 420 -> 450  (complete overlap, video)
  scoped_ptr<Cluster> cluster_a4(
      GenerateSingleStreamCluster(400, 470, kAudioTrackNum, 70));

  scoped_ptr<Cluster> cluster_v4(
      GenerateSingleStreamCluster(420, 450, kVideoTrackNum, 30));

  ASSERT_TRUE(AppendData(cluster_a4->data(), cluster_a4->size()));
  ASSERT_TRUE(AppendData(cluster_v4->data(), cluster_v4->size()));

  CheckExpectedRanges("{ [0,23) [120,150) [220,270) [320,350) [420,450) }");

  // Appending within buffered range should not affect buffered ranges.
  scoped_ptr<Cluster> cluster_a5(
      GenerateSingleStreamCluster(430, 450, kAudioTrackNum, 20));
  ASSERT_TRUE(AppendData(cluster_a5->data(), cluster_a5->size()));
  CheckExpectedRanges("{ [0,23) [120,150) [220,270) [320,350) [420,450) }");

  // Appending to single stream outside buffered ranges should not affect
  // buffered ranges.
  scoped_ptr<Cluster> cluster_v5(
      GenerateSingleStreamCluster(530, 540, kVideoTrackNum, 10));
  ASSERT_TRUE(AppendData(cluster_v5->data(), cluster_v5->size()));
  CheckExpectedRanges("{ [0,23) [120,150) [220,270) [320,350) [420,450) }");
}

// Once EndOfStream() is called, GetBufferedRanges should not cut off any
// over-hanging tails at the end of the ranges as this is likely due to block
// duration differences.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_EndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_ptr<Cluster> cluster_a(
      GenerateSingleStreamCluster(0, 90, kAudioTrackNum, 90));
  scoped_ptr<Cluster> cluster_v(
      GenerateSingleStreamCluster(0, 100, kVideoTrackNum, 100));

  ASSERT_TRUE(AppendData(cluster_a->data(), cluster_a->size()));
  ASSERT_TRUE(AppendData(cluster_v->data(), cluster_v->size()));

  CheckExpectedRanges("{ [0,90) }");

  demuxer_->EndOfStream(PIPELINE_OK);

  CheckExpectedRanges("{ [0,100) }");
}

TEST_F(ChunkDemuxerTest, TestDifferentStreamTimecodes) {
  ASSERT_TRUE(InitDemuxer(true, true, false));

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_OK));

  // Create a cluster where the video timecode begins 25ms after the audio.
  scoped_ptr<Cluster> start_cluster(
      GenerateCluster(0, 25, 8));

  ASSERT_TRUE(AppendData(start_cluster->data(), start_cluster->size()));
  GenerateExpectedReads(0, 25, 8, audio, video);

  // Seek to 5 seconds.
  demuxer_->StartWaitingForSeek();
  demuxer_->Seek(base::TimeDelta::FromSeconds(5),
                 NewExpectedStatusCB(PIPELINE_OK));

  // Generate a cluster to fulfill this seek, where audio timecode begins 25ms
  // after the video.
  scoped_ptr<Cluster> middle_cluster(GenerateCluster(5025, 5000, 8));
  ASSERT_TRUE(AppendData(middle_cluster->data(), middle_cluster->size()));
  GenerateExpectedReads(5025, 5000, 8, audio, video);
}

TEST_F(ChunkDemuxerTest, TestCodecPrefixMatching) {
  ChunkDemuxer::Status expected = ChunkDemuxer::kNotSupported;

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  expected = ChunkDemuxer::kOk;
#endif

  std::vector<std::string> codecs;
  codecs.push_back("avc1.4D4041");
  codecs.push_back("mp4a.40.2");

  EXPECT_EQ(demuxer_->AddId("source_id", "video/mp4", codecs), expected);
}

TEST_F(ChunkDemuxerTest, TestEndOfStreamFailures) {
  std::string audio_id = "audio";
  std::string video_id = "video";

  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  scoped_ptr<Cluster> cluster_a1(
      GenerateSingleStreamCluster(0, 15, kAudioTrackNum, 15));
  scoped_ptr<Cluster> cluster_v1(
      GenerateSingleStreamCluster(0, 5, kVideoTrackNum, 5));
  scoped_ptr<Cluster> cluster_v2(
      GenerateSingleStreamCluster(5, 10, kVideoTrackNum, 5));
  scoped_ptr<Cluster> cluster_v3(
      GenerateSingleStreamCluster(10, 20, kVideoTrackNum, 10));

  ASSERT_TRUE(AppendData(audio_id, cluster_a1->data(), cluster_a1->size()));
  ASSERT_TRUE(AppendData(video_id, cluster_v1->data(), cluster_v1->size()));
  ASSERT_TRUE(AppendData(video_id, cluster_v3->data(), cluster_v3->size()));

  CheckExpectedRanges(audio_id, "{ [0,15) }");
  CheckExpectedRanges(video_id, "{ [0,5) [10,20) }");

  // Make sure that end of stream fails because there is a gap between
  // the current position(0) and the end of the appended data.
  ASSERT_FALSE(demuxer_->EndOfStream(PIPELINE_OK));

  // Seek to an time that is inside the last ranges for both streams
  // and verify that the EndOfStream() is successful.
  demuxer_->StartWaitingForSeek();
  demuxer_->Seek(base::TimeDelta::FromMilliseconds(10),
                 NewExpectedStatusCB(PIPELINE_OK));

  ASSERT_TRUE(demuxer_->EndOfStream(PIPELINE_OK));

  // Seek back to 0 and verify that EndOfStream() fails again.
  demuxer_->StartWaitingForSeek();
  demuxer_->Seek(base::TimeDelta::FromMilliseconds(0),
                 NewExpectedStatusCB(PIPELINE_OK));

  ASSERT_FALSE(demuxer_->EndOfStream(PIPELINE_OK));

  // Append the missing range and verify that EndOfStream() succeeds now.
  ASSERT_TRUE(AppendData(video_id, cluster_v2->data(), cluster_v2->size()));

  CheckExpectedRanges(audio_id, "{ [0,15) }");
  CheckExpectedRanges(video_id, "{ [0,20) }");

  ASSERT_TRUE(demuxer_->EndOfStream(PIPELINE_OK));
}

TEST_F(ChunkDemuxerTest, TestGetBufferedRangesBeforeInitSegment) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Initialize(&host_, CreateInitDoneCB(PIPELINE_OK));
  ASSERT_EQ(AddId("audio", true, false), ChunkDemuxer::kOk);
  ASSERT_EQ(AddId("video", false, true), ChunkDemuxer::kOk);

  CheckExpectedRanges("audio", "{ }");
  CheckExpectedRanges("video", "{ }");
}

}  // namespace media
