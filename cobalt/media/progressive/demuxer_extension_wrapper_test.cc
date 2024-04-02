// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/progressive/demuxer_extension_wrapper.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cobalt/media/decoder_buffer_allocator.h"
#include "media/base/demuxer.h"
#include "starboard/extension/demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::ExplainMatchResult;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

// Matches a DemuxerStream and verifies that the type is |type|.
MATCHER_P(TypeIs, type, "") { return arg.type() == type; }

// Matches a DecoderBuffer and verifies that the data in the buffer is |data|.
MATCHER_P(BufferHasData, data, "") {
  return ExplainMatchResult(
      ElementsAreArray(data),
      std::tuple<const uint8_t*, size_t>{arg.data(), arg.data_size()},
      result_listener);
}

class MockDemuxerHost : public ::media::DemuxerHost {
 public:
  MockDemuxerHost() = default;

  MockDemuxerHost(const MockDemuxerHost&) = delete;
  MockDemuxerHost& operator=(const MockDemuxerHost&) = delete;

  ~MockDemuxerHost() override = default;

  MOCK_METHOD1(OnBufferedTimeRangesChanged,
               void(const ::media::Ranges<base::TimeDelta>&));
  MOCK_METHOD1(SetDuration, void(base::TimeDelta duration));
  MOCK_METHOD1(OnDemuxerError, void(::media::PipelineStatus error));
};

class MockDataSource : public DataSource {
 public:
  MockDataSource() {
    // Set reasonable default behavior for functions that are expected to
    // interact with callbacks and/or output parameters.
    ON_CALL(*this, Read(_, _, _, _))
        .WillByDefault(Invoke(+[](int64_t position, int size, uint8_t* data,
                                  const DataSource::ReadCB& read_cb) {
          memset(data, 0, size);
          read_cb.Run(size);
        }));
    ON_CALL(*this, GetSize(_)).WillByDefault(Invoke(+[](int64_t* size_out) {
      *size_out = 0;
      return true;
    }));
  }

  ~MockDataSource() override = default;

  MOCK_METHOD4(Read, void(int64_t position, int size, uint8_t* data,
                          const DataSource::ReadCB& read_cb));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Abort, void());
  MOCK_METHOD1(GetSize, bool(int64_t* size_out));
  MOCK_METHOD1(SetDownloadingStatusCB,
               void(const DownloadingStatusCB& downloading_status_cb));
};

// Mock class for receiving calls to the Cobalt Extension demuxer. Based on the
// CobaltExtensionDemuxer struct.
class MockCobaltExtensionDemuxer {
 public:
  MOCK_METHOD0(Initialize, CobaltExtensionDemuxerStatus());
  MOCK_METHOD1(Seek, CobaltExtensionDemuxerStatus(int64_t seek_time_us));
  MOCK_METHOD0(GetStartTime, int64_t());
  MOCK_METHOD0(GetTimelineOffset, int64_t());
  MOCK_METHOD3(Read, void(CobaltExtensionDemuxerStreamType type,
                          CobaltExtensionDemuxerReadCB read_cb,
                          void* read_cb_user_data));
  MOCK_METHOD1(GetAudioConfig,
               bool(CobaltExtensionDemuxerAudioDecoderConfig* config));
  MOCK_METHOD1(GetVideoConfig,
               bool(CobaltExtensionDemuxerVideoDecoderConfig* config));
  MOCK_METHOD0(GetDuration, int64_t());

  // Pure C functions to be used in CobaltExtensionDemuxer. These expect
  // |user_data| to be a pointer to a MockCobaltExtensionDemuxer.
  static CobaltExtensionDemuxerStatus InitializeImpl(void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)->Initialize();
  }

  static CobaltExtensionDemuxerStatus SeekImpl(int64_t seek_time_us,
                                               void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)->Seek(
        seek_time_us);
  }

  static int64_t GetStartTimeImpl(void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)->GetStartTime();
  }

  static int64_t GetTimelineOffsetImpl(void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)
        ->GetTimelineOffset();
  }
  static void ReadImpl(CobaltExtensionDemuxerStreamType type,
                       CobaltExtensionDemuxerReadCB read_cb,
                       void* read_cb_user_data, void* user_data) {
    static_cast<MockCobaltExtensionDemuxer*>(user_data)->Read(
        type, read_cb, read_cb_user_data);
  }

  static bool GetAudioConfigImpl(
      CobaltExtensionDemuxerAudioDecoderConfig* config, void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)->GetAudioConfig(
        config);
  }

  static bool GetVideoConfigImpl(
      CobaltExtensionDemuxerVideoDecoderConfig* config, void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)->GetVideoConfig(
        config);
  }

  static int64_t GetDurationImpl(void* user_data) {
    return static_cast<MockCobaltExtensionDemuxer*>(user_data)->GetDuration();
  }
};

// Forward declaration for the purpose of defining GetMockDemuxerApi. Defined
// below.
class MockDemuxerApi;

// Returns a pointer to a MockDemuxerApi with static storage duration. The
// returned pointer should not be deleted. Defined below.
MockDemuxerApi* GetMockDemuxerApi();

// Mock class for receiving calls to the Cobalt Extension demuxer API. Based on
// the CobaltExtensionDemuxerApi struct.
class MockDemuxerApi {
 public:
  MOCK_METHOD5(CreateDemuxer,
               CobaltExtensionDemuxer*(
                   CobaltExtensionDemuxerDataSource* data_source,
                   CobaltExtensionDemuxerAudioCodec* supported_audio_codecs,
                   int64_t supported_audio_codecs_size,
                   CobaltExtensionDemuxerVideoCodec* supported_video_codecs,
                   int64_t supported_video_codecs_size));
  MOCK_METHOD1(DestroyDemuxer, void(CobaltExtensionDemuxer* demuxer));

  // Pure C functions to be used in CobaltExtensionDemuxer. These expect
  // |user_data| to be a pointer to a MockDemuxerApi.
  static CobaltExtensionDemuxer* CreateDemuxerImpl(
      CobaltExtensionDemuxerDataSource* data_source,
      CobaltExtensionDemuxerAudioCodec* supported_audio_codecs,
      int64_t supported_audio_codecs_size,
      CobaltExtensionDemuxerVideoCodec* supported_video_codecs,
      int64_t supported_video_codecs_size) {
    return GetMockDemuxerApi()->CreateDemuxer(
        data_source, supported_audio_codecs, supported_audio_codecs_size,
        supported_video_codecs, supported_video_codecs_size);
  }

  static void DestroyDemuxerImpl(CobaltExtensionDemuxer* demuxer) {
    GetMockDemuxerApi()->DestroyDemuxer(demuxer);
  }
};

MockDemuxerApi* GetMockDemuxerApi() {
  static auto* const demuxer_api = []() {
    auto* inner_demuxer_api = new MockDemuxerApi;
    // This mock won't be destructed.
    testing::Mock::AllowLeak(inner_demuxer_api);
    return inner_demuxer_api;
  }();

  return demuxer_api;
}

CobaltExtensionDemuxer* CreateCDemuxer(
    MockCobaltExtensionDemuxer* mock_demuxer) {
  CHECK(mock_demuxer);
  return new CobaltExtensionDemuxer{
      &MockCobaltExtensionDemuxer::InitializeImpl,
      &MockCobaltExtensionDemuxer::SeekImpl,
      &MockCobaltExtensionDemuxer::GetStartTimeImpl,
      &MockCobaltExtensionDemuxer::GetTimelineOffsetImpl,
      &MockCobaltExtensionDemuxer::ReadImpl,
      &MockCobaltExtensionDemuxer::GetAudioConfigImpl,
      &MockCobaltExtensionDemuxer::GetVideoConfigImpl,
      &MockCobaltExtensionDemuxer::GetDurationImpl,
      mock_demuxer};
}

// A test fixture is used to verify and clear the global mock demuxer, and to
// manage the lifetime of the ScopedTaskEnvironment.
class DemuxerExtensionWrapperTest : public ::testing::Test {
 protected:
  DemuxerExtensionWrapperTest() = default;

  ~DemuxerExtensionWrapperTest() override {
    testing::Mock::VerifyAndClearExpectations(GetMockDemuxerApi());
  }

  // Waits |time_limit| for |done| to occur. Returns true if the event occurred,
  // false otherwise. While waiting, allows other threads to run and runs the
  // task runner until idle.
  bool WaitForEvent(base::WaitableEvent& done,
                    base::TimeDelta time_limit = base::Seconds(1)) {
    const base::Time deadline = base::Time::Now() + base::Seconds(1);
    while (base::Time::Now() < deadline) {
      task_environment_.RunUntilIdle();
      base::PlatformThread::YieldCurrentThread();
      if (done.IsSignaled()) {
        break;
      }
    }
    return done.IsSignaled();
  }

  // This must be deleted last.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT};
  // This is necessary in order to allocate DecoderBuffers. This is done
  // internally in DemuxerExtensionWrapper.
  DecoderBufferAllocator allocator_;
};

TEST_F(DemuxerExtensionWrapperTest, SuccessfullyInitializes) {
  // This must outlive the DemuxerExtensionWrapper.
  NiceMock<MockDemuxerHost> mock_host;
  MockDataSource data_source;
  MockDemuxerApi* mock_demuxer_api = GetMockDemuxerApi();  // Not owned.
  NiceMock<MockCobaltExtensionDemuxer> mock_demuxer;

  // In this test we don't care about what data is read.
  ON_CALL(mock_demuxer, Read(_, _, _))
      .WillByDefault(Invoke([](CobaltExtensionDemuxerStreamType type,
                               CobaltExtensionDemuxerReadCB read_cb,
                               void* read_cb_user_data) {
        // Simulate EOS.
        CobaltExtensionDemuxerBuffer eos_buffer = {};
        eos_buffer.end_of_stream = true;
        read_cb(&eos_buffer, read_cb_user_data);
      }));

  const CobaltExtensionDemuxerApi api = {
      /*name=*/kCobaltExtensionDemuxerApi,
      /*version=*/1,
      /*CreateDemuxer=*/&MockDemuxerApi::CreateDemuxerImpl,
      /*DestroyDemuxer=*/&MockDemuxerApi::DestroyDemuxerImpl,
  };

  auto c_demuxer =
      std::unique_ptr<CobaltExtensionDemuxer>(CreateCDemuxer(&mock_demuxer));
  EXPECT_CALL(*mock_demuxer_api, CreateDemuxer(_, _, _, _, _))
      .WillOnce(Return(c_demuxer.get()));
  EXPECT_CALL(*mock_demuxer_api, DestroyDemuxer(c_demuxer.get())).Times(1);

  std::unique_ptr<DemuxerExtensionWrapper> demuxer_wrapper =
      DemuxerExtensionWrapper::Create(
          &data_source, base::SequencedTaskRunner::GetCurrentDefault(), &api);

  ASSERT_THAT(demuxer_wrapper, NotNull());

  base::WaitableEvent init_done;
  base::MockCallback<base::OnceCallback<void(::media::PipelineStatus)>>
      initialize_cb;
  EXPECT_CALL(mock_demuxer, Initialize())
      .WillOnce(Return(kCobaltExtensionDemuxerOk));
  // Simulate an audio file.
  EXPECT_CALL(mock_demuxer, GetAudioConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerAudioDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecAAC;
        config->sample_format = kCobaltExtensionDemuxerSampleFormatF32;
        config->channel_layout = kCobaltExtensionDemuxerChannelLayoutStereo;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->samples_per_second = 44100;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(mock_demuxer, GetVideoConfig(NotNull())).WillOnce(Return(false));
  EXPECT_CALL(initialize_cb, Run(::media::PIPELINE_OK))
      .WillOnce(InvokeWithoutArgs([&init_done]() { init_done.Signal(); }));

  demuxer_wrapper->Initialize(&mock_host, initialize_cb.Get());

  EXPECT_TRUE(WaitForEvent(init_done));
}

TEST_F(DemuxerExtensionWrapperTest, ProvidesAudioAndVideoStreams) {
  // This must outlive the DemuxerExtensionWrapper.
  NiceMock<MockDemuxerHost> mock_host;
  MockDataSource data_source;
  MockDemuxerApi* mock_demuxer_api = GetMockDemuxerApi();  // Not owned.
  NiceMock<MockCobaltExtensionDemuxer> mock_demuxer;

  // In this test we don't care about what data is read.
  ON_CALL(mock_demuxer, Read(_, _, _))
      .WillByDefault(Invoke([](CobaltExtensionDemuxerStreamType type,
                               CobaltExtensionDemuxerReadCB read_cb,
                               void* read_cb_user_data) {
        // Simulate EOS.
        CobaltExtensionDemuxerBuffer eos_buffer = {};
        eos_buffer.end_of_stream = true;
        read_cb(&eos_buffer, read_cb_user_data);
      }));

  const CobaltExtensionDemuxerApi api = {
      /*name=*/kCobaltExtensionDemuxerApi,
      /*version=*/1,
      /*CreateDemuxer=*/&MockDemuxerApi::CreateDemuxerImpl,
      /*DestroyDemuxer=*/&MockDemuxerApi::DestroyDemuxerImpl,
  };

  auto c_demuxer =
      std::unique_ptr<CobaltExtensionDemuxer>(CreateCDemuxer(&mock_demuxer));
  EXPECT_CALL(*mock_demuxer_api, CreateDemuxer(_, _, _, _, _))
      .WillOnce(Return(c_demuxer.get()));
  EXPECT_CALL(*mock_demuxer_api, DestroyDemuxer(c_demuxer.get())).Times(1);

  std::unique_ptr<DemuxerExtensionWrapper> demuxer_wrapper =
      DemuxerExtensionWrapper::Create(
          &data_source, base::SequencedTaskRunner::GetCurrentDefault(), &api);

  ASSERT_THAT(demuxer_wrapper, NotNull());

  base::WaitableEvent init_done;
  base::MockCallback<base::OnceCallback<void(::media::PipelineStatus)>>
      initialize_cb;
  EXPECT_CALL(mock_demuxer, Initialize())
      .WillOnce(Return(kCobaltExtensionDemuxerOk));
  // Simulate an audio+video file.
  EXPECT_CALL(mock_demuxer, GetAudioConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerAudioDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecAAC;
        config->sample_format = kCobaltExtensionDemuxerSampleFormatF32;
        config->channel_layout = kCobaltExtensionDemuxerChannelLayoutStereo;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->samples_per_second = 44100;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(mock_demuxer, GetVideoConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerVideoDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecH264;
        config->profile = kCobaltExtensionDemuxerH264ProfileMain;
        config->color_space_primaries = 1;
        config->color_space_transfer = 1;
        config->color_space_matrix = 1;
        config->color_space_range_id =
            kCobaltExtensionDemuxerColorSpaceRangeIdFull;
        config->alpha_mode = kCobaltExtensionDemuxerHasAlpha;
        config->coded_width = 1920;
        config->coded_height = 1080;
        config->visible_rect_x = 0;
        config->visible_rect_y = 0;
        config->visible_rect_width = 1920;
        config->visible_rect_height = 1080;
        config->natural_width = 1920;
        config->natural_height = 1080;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(initialize_cb, Run(::media::PIPELINE_OK))
      .WillOnce(InvokeWithoutArgs([&init_done]() { init_done.Signal(); }));

  demuxer_wrapper->Initialize(&mock_host, initialize_cb.Get());

  EXPECT_TRUE(WaitForEvent(init_done));

  std::vector<::media::DemuxerStream*> streams =
      demuxer_wrapper->GetAllStreams();
  EXPECT_THAT(streams,
              UnorderedElementsAre(
                  Pointee(TypeIs(::media::DemuxerStream::Type::AUDIO)),
                  Pointee(TypeIs(::media::DemuxerStream::Type::VIDEO))));
}

TEST_F(DemuxerExtensionWrapperTest, ReadsAudioData) {
  // This must outlive the DemuxerExtensionWrapper.
  NiceMock<MockDemuxerHost> mock_host;
  MockDataSource data_source;
  MockDemuxerApi* mock_demuxer_api = GetMockDemuxerApi();  // Not owned.
  NiceMock<MockCobaltExtensionDemuxer> mock_demuxer;

  const CobaltExtensionDemuxerApi api = {
      /*name=*/kCobaltExtensionDemuxerApi,
      /*version=*/1,
      /*CreateDemuxer=*/&MockDemuxerApi::CreateDemuxerImpl,
      /*DestroyDemuxer=*/&MockDemuxerApi::DestroyDemuxerImpl,
  };

  auto c_demuxer =
      std::unique_ptr<CobaltExtensionDemuxer>(CreateCDemuxer(&mock_demuxer));
  EXPECT_CALL(*mock_demuxer_api, CreateDemuxer(_, _, _, _, _))
      .WillOnce(Return(c_demuxer.get()));
  EXPECT_CALL(*mock_demuxer_api, DestroyDemuxer(c_demuxer.get())).Times(1);

  std::unique_ptr<DemuxerExtensionWrapper> demuxer_wrapper =
      DemuxerExtensionWrapper::Create(
          &data_source, base::SequencedTaskRunner::GetCurrentDefault(), &api);

  ASSERT_THAT(demuxer_wrapper, NotNull());

  base::WaitableEvent init_done;
  base::MockCallback<base::OnceCallback<void(::media::PipelineStatus)>>
      initialize_cb;
  EXPECT_CALL(mock_demuxer, Initialize())
      .WillOnce(Return(kCobaltExtensionDemuxerOk));
  // Simulate an audio+video file.
  EXPECT_CALL(mock_demuxer, GetAudioConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerAudioDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecAAC;
        config->sample_format = kCobaltExtensionDemuxerSampleFormatF32;
        config->channel_layout = kCobaltExtensionDemuxerChannelLayoutStereo;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->samples_per_second = 44100;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(mock_demuxer, GetVideoConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerVideoDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecH264;
        config->profile = kCobaltExtensionDemuxerH264ProfileMain;
        config->color_space_primaries = 1;
        config->color_space_transfer = 1;
        config->color_space_matrix = 1;
        config->color_space_range_id =
            kCobaltExtensionDemuxerColorSpaceRangeIdFull;
        config->alpha_mode = kCobaltExtensionDemuxerHasAlpha;
        config->coded_width = 1920;
        config->coded_height = 1080;
        config->visible_rect_x = 0;
        config->visible_rect_y = 0;
        config->visible_rect_width = 1920;
        config->visible_rect_height = 1080;
        config->natural_width = 1920;
        config->natural_height = 1080;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(initialize_cb, Run(::media::PIPELINE_OK))
      .WillOnce(InvokeWithoutArgs([&init_done]() { init_done.Signal(); }));

  std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  EXPECT_CALL(mock_demuxer, Read(kCobaltExtensionDemuxerStreamTypeAudio, _, _))
      .WillOnce(Invoke([&buffer_data](CobaltExtensionDemuxerStreamType type,
                                      CobaltExtensionDemuxerReadCB read_cb,
                                      void* read_cb_user_data) {
        // Send one "real" buffer.
        CobaltExtensionDemuxerBuffer buffer = {};
        buffer.data = buffer_data.data();
        buffer.data_size = buffer_data.size();
        buffer.side_data = nullptr;
        buffer.side_data_elements = 0;
        buffer.pts = 0;
        buffer.duration = 1000;
        buffer.is_keyframe = true;
        buffer.end_of_stream = false;
        read_cb(&buffer, read_cb_user_data);
      }))
      .WillOnce(Invoke([](CobaltExtensionDemuxerStreamType type,
                          CobaltExtensionDemuxerReadCB read_cb,
                          void* read_cb_user_data) {
        // Simulate the audio stream being done.
        CobaltExtensionDemuxerBuffer eos_buffer = {};
        eos_buffer.end_of_stream = true;
        read_cb(&eos_buffer, read_cb_user_data);
      }));

  // The impl may or may not try reading video data. If it does, return EOS.
  EXPECT_CALL(mock_demuxer, Read(kCobaltExtensionDemuxerStreamTypeVideo, _, _))
      .Times(AtMost(1))
      .WillOnce(Invoke([](CobaltExtensionDemuxerStreamType type,
                          CobaltExtensionDemuxerReadCB read_cb,
                          void* read_cb_user_data) {
        // Simulate the video stream being done.
        CobaltExtensionDemuxerBuffer eos_buffer = {};
        eos_buffer.end_of_stream = true;
        read_cb(&eos_buffer, read_cb_user_data);
      }));

  demuxer_wrapper->Initialize(&mock_host, initialize_cb.Get());

  EXPECT_TRUE(WaitForEvent(init_done));

  std::vector<::media::DemuxerStream*> streams =
      demuxer_wrapper->GetAllStreams();
  ASSERT_THAT(streams,
              UnorderedElementsAre(
                  Pointee(TypeIs(::media::DemuxerStream::Type::AUDIO)),
                  Pointee(TypeIs(::media::DemuxerStream::Type::VIDEO))));
  ::media::DemuxerStream* audio_stream =
      streams[0]->type() == ::media::DemuxerStream::Type::AUDIO ? streams[0]
                                                                : streams[1];

  base::MockCallback<base::OnceCallback<void(
      ::media::DemuxerStream::Status,
      const std::vector<scoped_refptr<::media::DecoderBuffer>>&)>>
      read_cb;
  base::WaitableEvent read_done;
  std::vector<std::vector<uint8_t>> buffers;
  buffers.push_back(buffer_data);
  EXPECT_CALL(read_cb, Run(::media::DemuxerStream::kOk,
                           ElementsAre(Pointee(BufferHasData(buffer_data)))))
      .WillOnce(InvokeWithoutArgs([&read_done]() { read_done.Signal(); }));

  audio_stream->Read(1, read_cb.Get());
  EXPECT_TRUE(WaitForEvent(read_done));
}

TEST_F(DemuxerExtensionWrapperTest, ReadsVideoData) {
  // This must outlive the DemuxerExtensionWrapper.
  NiceMock<MockDemuxerHost> mock_host;
  MockDataSource data_source;
  MockDemuxerApi* mock_demuxer_api = GetMockDemuxerApi();  // Not owned.
  NiceMock<MockCobaltExtensionDemuxer> mock_demuxer;

  const CobaltExtensionDemuxerApi api = {
      /*name=*/kCobaltExtensionDemuxerApi,
      /*version=*/1,
      /*CreateDemuxer=*/&MockDemuxerApi::CreateDemuxerImpl,
      /*DestroyDemuxer=*/&MockDemuxerApi::DestroyDemuxerImpl,
  };

  auto c_demuxer =
      std::unique_ptr<CobaltExtensionDemuxer>(CreateCDemuxer(&mock_demuxer));
  EXPECT_CALL(*mock_demuxer_api, CreateDemuxer(_, _, _, _, _))
      .WillOnce(Return(c_demuxer.get()));
  EXPECT_CALL(*mock_demuxer_api, DestroyDemuxer(c_demuxer.get())).Times(1);

  std::unique_ptr<DemuxerExtensionWrapper> demuxer_wrapper =
      DemuxerExtensionWrapper::Create(
          &data_source, base::SequencedTaskRunner::GetCurrentDefault(), &api);

  ASSERT_THAT(demuxer_wrapper, NotNull());

  base::WaitableEvent init_done;
  base::MockCallback<base::OnceCallback<void(::media::PipelineStatus)>>
      initialize_cb;
  EXPECT_CALL(mock_demuxer, Initialize())
      .WillOnce(Return(kCobaltExtensionDemuxerOk));
  // Simulate an audio+video file.
  EXPECT_CALL(mock_demuxer, GetAudioConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerAudioDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecAAC;
        config->sample_format = kCobaltExtensionDemuxerSampleFormatF32;
        config->channel_layout = kCobaltExtensionDemuxerChannelLayoutStereo;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->samples_per_second = 44100;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(mock_demuxer, GetVideoConfig(NotNull()))
      .WillOnce(Invoke([](CobaltExtensionDemuxerVideoDecoderConfig* config) {
        config->codec = kCobaltExtensionDemuxerCodecH264;
        config->profile = kCobaltExtensionDemuxerH264ProfileMain;
        config->color_space_primaries = 1;
        config->color_space_transfer = 1;
        config->color_space_matrix = 1;
        config->color_space_range_id =
            kCobaltExtensionDemuxerColorSpaceRangeIdFull;
        config->alpha_mode = kCobaltExtensionDemuxerHasAlpha;
        config->coded_width = 1920;
        config->coded_height = 1080;
        config->visible_rect_x = 0;
        config->visible_rect_y = 0;
        config->visible_rect_width = 1920;
        config->visible_rect_height = 1080;
        config->natural_width = 1920;
        config->natural_height = 1080;
        config->encryption_scheme =
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
        config->extra_data = nullptr;
        config->extra_data_size = 0;

        return true;
      }));
  EXPECT_CALL(initialize_cb, Run(::media::PIPELINE_OK))
      .WillOnce(InvokeWithoutArgs([&init_done]() { init_done.Signal(); }));

  std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  EXPECT_CALL(mock_demuxer, Read(kCobaltExtensionDemuxerStreamTypeVideo, _, _))
      .WillOnce(Invoke([&buffer_data](CobaltExtensionDemuxerStreamType type,
                                      CobaltExtensionDemuxerReadCB read_cb,
                                      void* read_cb_user_data) {
        // Send one "real" buffer.
        CobaltExtensionDemuxerBuffer buffer = {};
        buffer.data = buffer_data.data();
        buffer.data_size = buffer_data.size();
        buffer.side_data = nullptr;
        buffer.side_data_elements = 0;
        buffer.pts = 0;
        buffer.duration = 1000;
        buffer.is_keyframe = true;
        buffer.end_of_stream = false;
        read_cb(&buffer, read_cb_user_data);
      }))
      .WillOnce(Invoke([](CobaltExtensionDemuxerStreamType type,
                          CobaltExtensionDemuxerReadCB read_cb,
                          void* read_cb_user_data) {
        // Simulate the video stream being done.
        CobaltExtensionDemuxerBuffer eos_buffer = {};
        eos_buffer.end_of_stream = true;
        read_cb(&eos_buffer, read_cb_user_data);
      }));

  // The impl may or may not try reading audio data. If it does, return EOS.
  EXPECT_CALL(mock_demuxer, Read(kCobaltExtensionDemuxerStreamTypeAudio, _, _))
      .Times(AtMost(1))
      .WillOnce(Invoke([](CobaltExtensionDemuxerStreamType type,
                          CobaltExtensionDemuxerReadCB read_cb,
                          void* read_cb_user_data) {
        // Simulate the audio stream being done.
        CobaltExtensionDemuxerBuffer eos_buffer = {};
        eos_buffer.end_of_stream = true;
        read_cb(&eos_buffer, read_cb_user_data);
      }));

  demuxer_wrapper->Initialize(&mock_host, initialize_cb.Get());

  EXPECT_TRUE(WaitForEvent(init_done));

  std::vector<::media::DemuxerStream*> streams =
      demuxer_wrapper->GetAllStreams();
  ASSERT_THAT(streams,
              UnorderedElementsAre(
                  Pointee(TypeIs(::media::DemuxerStream::Type::AUDIO)),
                  Pointee(TypeIs(::media::DemuxerStream::Type::VIDEO))));
  ::media::DemuxerStream* video_stream =
      streams[0]->type() == ::media::DemuxerStream::Type::VIDEO ? streams[0]
                                                                : streams[1];

  base::MockCallback<base::OnceCallback<void(
      ::media::DemuxerStream::Status,
      const std::vector<scoped_refptr<::media::DecoderBuffer>>&)>>
      read_cb;
  base::WaitableEvent read_done;
  std::vector<std::vector<uint8_t>> buffers;
  buffers.push_back(buffer_data);
  EXPECT_CALL(read_cb, Run(::media::DemuxerStream::kOk,
                           ElementsAre(Pointee(BufferHasData(buffer_data)))))
      .WillOnce(InvokeWithoutArgs([&read_done]() { read_done.Signal(); }));

  video_stream->Read(1, read_cb.Get());
  EXPECT_TRUE(WaitForEvent(read_done));
}

}  // namespace
}  // namespace media
}  // namespace cobalt
