// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_ffmpeg.h"
#include "media/filters/chunk_demuxer.h"
#include "media/webm/cluster_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
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

class ChunkDemuxerTest : public testing::Test{
 protected:
  enum CodecsIndex {
    AUDIO,
    VIDEO,
    MAX_CODECS_INDEX
  };

  ChunkDemuxerTest()
      : demuxer_(new ChunkDemuxer()) {
    memset(&format_context_, 0, sizeof(format_context_));
    memset(&streams_, 0, sizeof(streams_));
    memset(&codecs_, 0, sizeof(codecs_));

    codecs_[VIDEO].codec_type = CODEC_TYPE_VIDEO;
    codecs_[VIDEO].codec_id = CODEC_ID_VP8;
    codecs_[VIDEO].width = 320;
    codecs_[VIDEO].height = 240;

    codecs_[AUDIO].codec_type = CODEC_TYPE_AUDIO;
    codecs_[AUDIO].codec_id = CODEC_ID_VORBIS;
    codecs_[AUDIO].channels = 2;
    codecs_[AUDIO].sample_rate = 44100;
  }

  virtual ~ChunkDemuxerTest() {
    if (demuxer_.get())
      demuxer_->Shutdown();
  }

  void ReadFile(const std::string& name, scoped_array<uint8>* buffer,
                int* size) {
    FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"))
        .AppendASCII(name);

    int64 tmp = 0;
    EXPECT_TRUE(file_util::GetFileSize(file_path, &tmp));
    EXPECT_LT(tmp, 32768);
    int file_size = static_cast<int>(tmp);

    buffer->reset(new uint8[file_size]);
    EXPECT_EQ(file_size,
              file_util::ReadFile(file_path,
                                  reinterpret_cast<char*>(buffer->get()),
                                  file_size));
    *size = file_size;
  }

  void CreateInfoTracks(bool has_audio, bool has_video,
                        scoped_array<uint8>* buffer, int* size) {
    scoped_array<uint8> info;
    int info_size = 0;
    scoped_array<uint8> audio_track_entry;
    int audio_track_entry_size = 0;
    scoped_array<uint8> video_track_entry;
    int video_track_entry_size = 0;

    ReadFile("webm_info_element", &info, &info_size);
    ReadFile("webm_vorbis_track_entry", &audio_track_entry,
             &audio_track_entry_size);
    ReadFile("webm_vp8_track_entry", &video_track_entry,
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

  void SetupAVFormatContext(bool has_audio, bool has_video) {
    int i = 0;
    if (has_audio) {
      format_context_.streams[i] = &streams_[i];
      streams_[i].codec = &codecs_[AUDIO];
      streams_[i].duration = 100;
      streams_[i].time_base.den = base::Time::kMicrosecondsPerSecond;
      streams_[i].time_base.num = 1;
      i++;
    }

    if (has_video) {
      format_context_.streams[i] = &streams_[i];
      streams_[i].codec = &codecs_[VIDEO];
      streams_[i].duration = 100;
      streams_[i].time_base.den = base::Time::kMicrosecondsPerSecond;
      streams_[i].time_base.num = 1;
      i++;
    }

    format_context_.nb_streams = i;
  }

void InitDemuxer(bool has_audio, bool has_video) {
    EXPECT_CALL(mock_ffmpeg_, AVOpenInputFile(_, _, NULL, 0, NULL))
        .WillOnce(DoAll(SetArgumentPointee<0>(&format_context_),
                        Return(0)));

    EXPECT_CALL(mock_ffmpeg_, AVFindStreamInfo(&format_context_))
        .WillOnce(Return(0));

    EXPECT_CALL(mock_ffmpeg_, AVCloseInputFile(&format_context_));

    EXPECT_CALL(mock_ffmpeg_, AVRegisterLockManager(_))
        .WillRepeatedly(Return(0));

    scoped_array<uint8> info_tracks;
    int info_tracks_size = 0;
    CreateInfoTracks(has_audio, has_video, &info_tracks, &info_tracks_size);

    SetupAVFormatContext(has_audio, has_video);

    EXPECT_EQ(demuxer_->Init(info_tracks.get(), info_tracks_size),
              has_audio || has_video);
  }

  void AddSimpleBlock(ClusterBuilder* cb, int track_num, int64 timecode) {
    uint8 data[] = { 0x00 };
    cb->AddSimpleBlock(track_num, timecode, 0, data, sizeof(data));
  }

  MOCK_METHOD1(Checkpoint, void(int id));

  MockFFmpeg mock_ffmpeg_;

  AVFormatContext format_context_;
  AVCodecContext codecs_[MAX_CODECS_INDEX];
  AVStream streams_[MAX_CODECS_INDEX];

  scoped_refptr<ChunkDemuxer> demuxer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerTest);
};

TEST_F(ChunkDemuxerTest, TestInit) {
  // Test no streams, audio-only, video-only, and audio & video scenarios.
  for (int i = 0; i < 4; i++) {
    bool has_audio = (i & 0x1) != 0;
    bool has_video = (i & 0x2) != 0;

    demuxer_ = new ChunkDemuxer();
    InitDemuxer(has_audio, has_video);
    EXPECT_EQ(demuxer_->GetStream(DemuxerStream::AUDIO).get() != NULL,
              has_audio);
    EXPECT_EQ(demuxer_->GetStream(DemuxerStream::VIDEO).get() != NULL,
              has_video);
    demuxer_->Shutdown();
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
TEST_F(ChunkDemuxerTest, TestAddDataAfterSeek) {
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

  EXPECT_TRUE(demuxer_->AddData(cluster->data(), cluster->size()));

  Checkpoint(2);
}

// Test the case where AddData() is called before Init(). This can happen
// when JavaScript starts sending data before the pipeline is completely
// initialized.
TEST_F(ChunkDemuxerTest, TestAddDataBeforeInit) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  scoped_ptr<Cluster> cluster(cb.Finish());

  EXPECT_TRUE(demuxer_->AddData(cluster->data(), cluster->size()));

  InitDemuxer(true, true);

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_OK));
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

  EXPECT_TRUE(demuxer_->AddData(cluster->data(), cluster->size()));

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

  EXPECT_TRUE(demuxer_->AddData(clusterA->data(), clusterA->size()));

  // Cluster B starts before clusterA and has data
  // that overlaps.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  AddSimpleBlock(&cb, kAudioTrackNum, 28);
  AddSimpleBlock(&cb, kVideoTrackNum, 40);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  // Make sure that AddData() fails because this cluster data
  // is before previous data.
  EXPECT_FALSE(demuxer_->AddData(clusterB->data(), clusterB->size()));

  // Cluster C starts after clusterA.
  cb.SetClusterTimecode(56);
  AddSimpleBlock(&cb, kAudioTrackNum, 56);
  AddSimpleBlock(&cb, kVideoTrackNum, 76);
  AddSimpleBlock(&cb, kAudioTrackNum, 79);
  AddSimpleBlock(&cb, kVideoTrackNum, 109);
  scoped_ptr<Cluster> clusterC(cb.Finish());

  // Verify that clusterC is accepted.
  EXPECT_TRUE(demuxer_->AddData(clusterC->data(), clusterC->size()));

  // Flush and try clusterB again.
  demuxer_->FlushData();
  EXPECT_TRUE(demuxer_->AddData(clusterB->data(), clusterB->size()));

  // Following that with clusterC should work too since it doesn't
  // overlap with clusterB.
  EXPECT_TRUE(demuxer_->AddData(clusterC->data(), clusterC->size()));
}

TEST_F(ChunkDemuxerTest, TestInvalidBlockSequences) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test the case where timecode is not monotonically
  // increasing but stays above the cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 7);
  AddSimpleBlock(&cb, kVideoTrackNum, 15);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  EXPECT_FALSE(demuxer_->AddData(clusterA->data(), clusterA->size()));

  // Test timecodes going backwards before cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 3);
  AddSimpleBlock(&cb, kVideoTrackNum, 3);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  EXPECT_FALSE(demuxer_->AddData(clusterB->data(), clusterB->size()));

  // Test strict monotonic increasing timestamps on a per stream
  // basis.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> clusterC(cb.Finish());

  EXPECT_FALSE(demuxer_->AddData(clusterC->data(), clusterC->size()));

  // Test strict monotonic increasing timestamps on a per stream
  // basis across clusters.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  scoped_ptr<Cluster> clusterD(cb.Finish());

  EXPECT_TRUE(demuxer_->AddData(clusterD->data(), clusterD->size()));

  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> clusterE(cb.Finish());

  EXPECT_FALSE(demuxer_->AddData(clusterE->data(), clusterE->size()));
}

}  // namespace media
