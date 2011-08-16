// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/webm/cluster_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::NiceMock;
using ::testing::_;

namespace media {

static const uint8 kTracksHeader[] = {
  0x16, 0x54, 0xAE, 0x6B, // Tracks ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // tracks(size = 0)
};

static const int kTracksHeaderSize = sizeof(kTracksHeader);
static const int kTracksSizeOffset = 4;

static const int kVideoTrackNum = 1;
static const int kAudioTrackNum = 2;

class MockChunkDemuxerClient : public ChunkDemuxerClient {
 public:
  MockChunkDemuxerClient() {}
  virtual ~MockChunkDemuxerClient() {}

  MOCK_METHOD1(DemuxerOpened, void(ChunkDemuxer* demuxer));
  MOCK_METHOD0(DemuxerClosed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChunkDemuxerClient);
};

class ChunkDemuxerTest : public testing::Test{
 protected:
  enum CodecsIndex {
    AUDIO,
    VIDEO,
    MAX_CODECS_INDEX
  };

  ChunkDemuxerTest()
      : client_(new MockChunkDemuxerClient()),
        demuxer_(new ChunkDemuxer(client_.get())) {
  }

  virtual ~ChunkDemuxerTest() {
    ShutdownDemuxer();
  }

  void CreateInfoTracks(bool has_audio, bool has_video,
                        scoped_array<uint8>* buffer, int* size) {
    scoped_array<uint8> info;
    int info_size = 0;
    scoped_array<uint8> audio_track_entry;
    int audio_track_entry_size = 0;
    scoped_array<uint8> video_track_entry;
    int video_track_entry_size = 0;

    ReadTestDataFile("webm_info_element", &info, &info_size);
    ReadTestDataFile("webm_vorbis_track_entry", &audio_track_entry,
                     &audio_track_entry_size);
    ReadTestDataFile("webm_vp8_track_entry", &video_track_entry,
                     &video_track_entry_size);

    int tracks_element_size = 0;

    if (has_audio)
      tracks_element_size += audio_track_entry_size;

    if (has_video)
      tracks_element_size += video_track_entry_size;

    *size = info_size + kTracksHeaderSize + tracks_element_size;

    buffer->reset(new uint8[*size]);

    uint8* buf = buffer->get();
    memcpy(buf, info.get(), info_size);
    buf += info_size;

    memcpy(buf, kTracksHeader, kTracksHeaderSize);

    int tmp = tracks_element_size;
    for (int i = 7; i > 0; i--) {
      buf[kTracksSizeOffset + i] = tmp & 0xff;
      tmp >>= 8;
    }

    buf += kTracksHeaderSize;

    if (has_audio) {
      memcpy(buf, audio_track_entry.get(), audio_track_entry_size);
      buf += audio_track_entry_size;
    }

    if (has_video) {
      memcpy(buf, video_track_entry.get(), video_track_entry_size);
      buf += video_track_entry_size;
    }
  }

  void AppendData(const uint8* data, unsigned length) {
    EXPECT_CALL(mock_filter_host_, SetBufferedBytes(_)).Times(AnyNumber());
    EXPECT_CALL(mock_filter_host_, SetBufferedTime(_)).Times(AnyNumber());
    EXPECT_CALL(mock_filter_host_, SetNetworkActivity(true)).Times(AnyNumber());
    EXPECT_TRUE(demuxer_->AppendData(data, length));
  }

  void AppendInfoTracks(bool has_audio, bool has_video) {
    scoped_array<uint8> info_tracks;
    int info_tracks_size = 0;
    CreateInfoTracks(has_audio, has_video, &info_tracks, &info_tracks_size);

    AppendData(info_tracks.get(), info_tracks_size);
  }

  static void InitDoneCalled(bool* was_called, PipelineStatus expectedStatus,
                             PipelineStatus status) {
    EXPECT_EQ(status, expectedStatus);
    *was_called = true;
  }

  void InitDemuxer(bool has_audio, bool has_video) {
    bool init_done_called = false;
    PipelineStatus expectedStatus =
        (has_audio || has_video) ? PIPELINE_OK : DEMUXER_ERROR_COULD_NOT_OPEN;

    EXPECT_CALL(*client_, DemuxerOpened(_));
    demuxer_->Init(base::Bind(&ChunkDemuxerTest::InitDoneCalled,
                              &init_done_called,
                              expectedStatus));

    EXPECT_FALSE(init_done_called);

    AppendInfoTracks(has_audio, has_video);

    EXPECT_TRUE(init_done_called);
    EXPECT_CALL(mock_filter_host_, SetDuration(_));
    EXPECT_CALL(mock_filter_host_, SetCurrentReadPosition(_));
    demuxer_->set_host(&mock_filter_host_);
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

  MOCK_METHOD1(Checkpoint, void(int id));

  MockFilterHost mock_filter_host_;

  scoped_ptr<MockChunkDemuxerClient> client_;
  scoped_refptr<ChunkDemuxer> demuxer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerTest);
};

TEST_F(ChunkDemuxerTest, TestInit) {
  // Test no streams, audio-only, video-only, and audio & video scenarios.
  for (int i = 0; i < 4; i++) {
    bool has_audio = (i & 0x1) != 0;
    bool has_video = (i & 0x2) != 0;

    client_.reset(new MockChunkDemuxerClient());
    demuxer_ = new ChunkDemuxer(client_.get());
    InitDemuxer(has_audio, has_video);
    EXPECT_EQ(demuxer_->GetStream(DemuxerStream::AUDIO).get() != NULL,
              has_audio);
    EXPECT_EQ(demuxer_->GetStream(DemuxerStream::VIDEO).get() != NULL,
              has_video);
    ShutdownDemuxer();
    demuxer_ = NULL;
  }
}

// Makes sure that  Seek() reports an error if Shutdown()
// is called before the first cluster is passed to the demuxer.
TEST_F(ChunkDemuxerTest, TestShutdownBeforeFirstSeekCompletes) {
  InitDemuxer(true, true);

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_ERROR_ABORT));
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, TestAppendDataAfterSeek) {
  InitDemuxer(true, true);

  InSequence s;

  EXPECT_CALL(*this, Checkpoint(1));

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_OK));

  EXPECT_CALL(*this, Checkpoint(2));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  scoped_ptr<Cluster> cluster(cb.Finish());

  Checkpoint(1);

  AppendData(cluster->data(), cluster->size());

  Checkpoint(2);
}

// Test the case where AppendData() is called before Init().
TEST_F(ChunkDemuxerTest, TestAppendDataBeforeInit) {
  scoped_array<uint8> info_tracks;
  int info_tracks_size = 0;
  CreateInfoTracks(true, true, &info_tracks, &info_tracks_size);

  EXPECT_FALSE(demuxer_->AppendData(info_tracks.get(), info_tracks_size));
}

static void OnReadDone(const base::TimeDelta& expected_time,
                       bool* called,
                       Buffer* buffer) {
  EXPECT_EQ(expected_time, buffer->GetTimestamp());
  *called = true;
}

// Make sure Read() callbacks are dispatched with the proper data.
TEST_F(ChunkDemuxerTest, TestRead) {
  InitDemuxer(true, true);

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done = false;
  bool video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(32),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(123),
                         &video_read_done));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 32);
  AddSimpleBlock(&cb, kVideoTrackNum, 123);
  scoped_ptr<Cluster> cluster(cb.Finish());

  AppendData(cluster->data(), cluster->size());

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, TestOutOfOrderClusters) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  cb.SetClusterTimecode(10);
  AddSimpleBlock(&cb, kAudioTrackNum, 10);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 33);
  AddSimpleBlock(&cb, kVideoTrackNum, 43);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  AppendData(clusterA->data(), clusterA->size());

  // Cluster B starts before clusterA and has data
  // that overlaps.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  AddSimpleBlock(&cb, kAudioTrackNum, 28);
  AddSimpleBlock(&cb, kVideoTrackNum, 40);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  // Make sure that AppendData() fails because this cluster data
  // is before previous data.
  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterB->data(), clusterB->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(45);
  AddSimpleBlock(&cb, kAudioTrackNum, 45);
  AddSimpleBlock(&cb, kVideoTrackNum, 45);
  scoped_ptr<Cluster> clusterC(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterC->data(), clusterC->size()));
}

TEST_F(ChunkDemuxerTest, TestNonMonotonicButAboveClusterTimecode) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test the case where block timecodes are not monotonically
  // increasing but stay above the cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 7);
  AddSimpleBlock(&cb, kVideoTrackNum, 15);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterA->data(), clusterA->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(20);
  AddSimpleBlock(&cb, kAudioTrackNum, 20);
  AddSimpleBlock(&cb, kVideoTrackNum, 20);
  scoped_ptr<Cluster> clusterB(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterB->data(), clusterB->size()));
}

TEST_F(ChunkDemuxerTest, TestBackwardsAndBeforeClusterTimecode) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test timecodes going backwards and including values less than the cluster
  // timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 3);
  AddSimpleBlock(&cb, kVideoTrackNum, 3);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterA->data(), clusterA->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(6);
  AddSimpleBlock(&cb, kAudioTrackNum, 6);
  AddSimpleBlock(&cb, kVideoTrackNum, 6);
  scoped_ptr<Cluster> clusterB(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterB->data(), clusterB->size()));
}


TEST_F(ChunkDemuxerTest, TestPerStreamMonotonicallyIncreasingTimestamps) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test strict monotonic increasing timestamps on a per stream
  // basis.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> cluster(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(cluster->data(), cluster->size());
}

TEST_F(ChunkDemuxerTest, TestMonotonicallyIncreasingTimestampsAcrossClusters) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test strict monotonic increasing timestamps on a per stream
  // basis across clusters.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  AppendData(clusterA->data(), clusterA->size());

  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterB->data(), clusterB->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(10);
  AddSimpleBlock(&cb, kAudioTrackNum, 10);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  scoped_ptr<Cluster> clusterC(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterC->data(), clusterC->size()));
}

// Test the case where a cluster is passed to AppendData() before
// INFO & TRACKS data.
TEST_F(ChunkDemuxerTest, TestClusterBeforeInfoTracks) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Init(NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  scoped_ptr<Cluster> cluster(cb.Finish());

  AppendData(cluster->data(), cluster->size());
}

// Test cases where we get an EndOfStream() call during initialization.
TEST_F(ChunkDemuxerTest, TestEOSDuringInit) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Init(NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));
  demuxer_->EndOfStream(PIPELINE_OK);
}

TEST_F(ChunkDemuxerTest, TestDecodeErrorEndOfStream) {
  InitDemuxer(true, true);

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  AddSimpleBlock(&cb, kAudioTrackNum, 23);
  AddSimpleBlock(&cb, kVideoTrackNum, 33);
  scoped_ptr<Cluster> cluster(cb.Finish());
  AppendData(cluster->data(), cluster->size());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  demuxer_->EndOfStream(PIPELINE_ERROR_DECODE);
}

TEST_F(ChunkDemuxerTest, TestNetworkErrorEndOfStream) {
  InitDemuxer(true, true);

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  AddSimpleBlock(&cb, kAudioTrackNum, 23);
  AddSimpleBlock(&cb, kVideoTrackNum, 33);
  scoped_ptr<Cluster> cluster(cb.Finish());
  AppendData(cluster->data(), cluster->size());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_NETWORK));
  demuxer_->EndOfStream(PIPELINE_ERROR_NETWORK);
}

}  // namespace media
