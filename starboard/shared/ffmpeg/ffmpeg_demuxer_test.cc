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

#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <tuple>
#include <vector>

#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace ffmpeg {
namespace {

using ::testing::_;
using ::testing::AllArgs;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArg;

// Compares two CobaltExtensionDemuxerSideData structs. Deep equality is
// checked; in other words, the actual side data is inspected (not just the ptr
// addresses).
MATCHER_P(SideDataEq, expected, "") {
  return arg.type == expected.type && arg.data_size == expected.data_size &&
         ExplainMatchResult(
             ElementsAreArray(expected.data,
                              static_cast<size_t>(expected.data_size)),
             std::tuple<uint8_t*, size_t>{arg.data, arg.data_size},
             result_listener);
}

// Compares two CobaltExtensionDemuxerBuffers. Deep equality is checked; the
// side data and data are checked element-by-element, rather than simply
// checking ptr addresses.
MATCHER_P(BufferMatches, expected_buffer, "") {
  if (expected_buffer.end_of_stream) {
    // For EoS buffers, we don't care about the other values.
    return arg.end_of_stream;
  }

  if (arg.data_size != expected_buffer.data_size) {
    return false;
  }
  if (!ExplainMatchResult(
          ElementsAreArray(expected_buffer.data,
                           static_cast<size_t>(expected_buffer.data_size)),
          std::tuple<uint8_t*, size_t>{arg.data, arg.data_size},
          result_listener)) {
    return false;
  }
  if (arg.side_data_elements != expected_buffer.side_data_elements) {
    return false;
  }
  // Note: ::testing::Pointwise doesn't support pointer/count as a
  // representation of an array, so we manually check each side data element.
  for (int i = 0; i < expected_buffer.side_data_elements; ++i) {
    if (!ExplainMatchResult(SideDataEq(expected_buffer.side_data[i]),
                            arg.side_data[i], result_listener)) {
      return false;
    }
  }
  return (arg.pts == expected_buffer.pts &&
          arg.duration == expected_buffer.duration &&
          arg.is_keyframe == expected_buffer.is_keyframe &&
          arg.end_of_stream == expected_buffer.end_of_stream);
}

// Streaming is not supported.
constexpr bool kIsStreaming = false;

// Used to convert a MockFn to a pure C function.
template <typename T, typename U>
void CallMockCB(U* u, void* user_data) {
  static_cast<T*>(user_data)->AsStdFunction()(u);
}

// A mock class for receiving FFmpeg calls. The API mimics the relevant parts of
// the real FFmpeg API.
class MockFFmpegImpl {
 public:
  MOCK_METHOD1(AVCodecFreeContext, void(AVCodecContext** avctx));
  MOCK_METHOD1(AVFree, void(void* ptr));
  MOCK_METHOD4(AVRescaleRnd, int64_t(int64_t a, int64_t b, int64_t c, int rnd));
  MOCK_METHOD4(AVDictGet,
               AVDictionaryEntry*(const AVDictionary* m,
                                  const char* key,
                                  const AVDictionaryEntry* prev,
                                  int flags));
  MOCK_METHOD4(AVFormatOpenInput,
               int(AVFormatContext** ps,
                   const char* filename,
                   AVInputFormat* fmt,
                   AVDictionary** options));
  MOCK_METHOD1(AVFormatCloseInput, void(AVFormatContext** s));
  MOCK_METHOD7(
      AVIOAllocContext,
      AVIOContext*(
          unsigned char* buffer,
          int buffer_size,
          int write_flag,
          void* opaque,
          int (*read_packet)(void* opaque, uint8_t* buf, int buf_size),
          int (*write_packet)(void* opaque, uint8_t* buf, int buf_size),
          int64_t (*seek)(void* opaque, int64_t offset, int whence)));
  MOCK_METHOD1(AVMalloc, void*(size_t size));
  MOCK_METHOD0(AVFormatAllocContext, AVFormatContext*());
  MOCK_METHOD2(AVFormatFindStreamInfo,
               int(AVFormatContext* ic, AVDictionary** options));
  MOCK_METHOD4(
      AVSeekFrame,
      int(AVFormatContext* s, int stream_index, int64_t timestamp, int flags));
  MOCK_METHOD0(AVPacketAlloc, AVPacket*());
  MOCK_METHOD1(AVPacketFree, void(AVPacket** pkt));
  MOCK_METHOD1(AVPacketUnref, void(AVPacket* pkt));
  MOCK_METHOD2(AVReadFrame, int(AVFormatContext* s, AVPacket* pkt));
  MOCK_METHOD1(AVCodecAllocContext3, AVCodecContext*(const AVCodec* codec));
  MOCK_METHOD2(AVCodecParametersToContext,
               int(AVCodecContext* codec, const AVCodecParameters* par));
};

// Returns a MockFFmpegImpl instance. It should not be deleted by the caller.
MockFFmpegImpl* GetMockFFmpegImpl() {
  static auto* const ffmpeg_wrapper = []() {
    auto* wrapper = new MockFFmpegImpl;
    // This mock won't be destructed.
    testing::Mock::AllowLeak(wrapper);
    return wrapper;
  }();
  return ffmpeg_wrapper;
}

// Pure C functions that call the static mock.
void mock_avcodec_free_context(AVCodecContext** avctx) {
  GetMockFFmpegImpl()->AVCodecFreeContext(avctx);
}

void mock_av_free(void* ptr) {
  GetMockFFmpegImpl()->AVFree(ptr);
}

int64_t mock_av_rescale_rnd(int64_t a, int64_t b, int64_t c, int rnd) {
  return GetMockFFmpegImpl()->AVRescaleRnd(a, b, c, rnd);
}

AVDictionaryEntry* mock_av_dict_get(const AVDictionary* m,
                                    const char* key,
                                    const AVDictionaryEntry* prev,
                                    int flags) {
  return GetMockFFmpegImpl()->AVDictGet(m, key, prev, flags);
}

int mock_avformat_open_input(AVFormatContext** ps,
                             const char* filename,
                             AVInputFormat* fmt,
                             AVDictionary** options) {
  return GetMockFFmpegImpl()->AVFormatOpenInput(ps, filename, fmt, options);
}

void mock_avformat_close_input(AVFormatContext** s) {
  GetMockFFmpegImpl()->AVFormatCloseInput(s);
}

AVIOContext* mock_avio_alloc_context(
    unsigned char* buffer,
    int buffer_size,
    int write_flag,
    void* opaque,
    int (*read_packet)(void* opaque, uint8_t* buf, int buf_size),
    int (*write_packet)(void* opaque, uint8_t* buf, int buf_size),
    int64_t (*seek)(void* opaque, int64_t offset, int whence)) {
  return GetMockFFmpegImpl()->AVIOAllocContext(
      buffer, buffer_size, write_flag, opaque, read_packet, write_packet, seek);
}

void* mock_av_malloc(size_t size) {
  return GetMockFFmpegImpl()->AVMalloc(size);
}

AVFormatContext* mock_avformat_alloc_context() {
  return GetMockFFmpegImpl()->AVFormatAllocContext();
}

int mock_avformat_find_stream_info(AVFormatContext* ic,
                                   AVDictionary** options) {
  return GetMockFFmpegImpl()->AVFormatFindStreamInfo(ic, options);
}

int mock_av_seek_frame(AVFormatContext* s,
                       int stream_index,
                       int64_t timestamp,
                       int flags) {
  return GetMockFFmpegImpl()->AVSeekFrame(s, stream_index, timestamp, flags);
}

AVPacket* mock_av_packet_alloc() {
  return GetMockFFmpegImpl()->AVPacketAlloc();
}

void mock_av_packet_free(AVPacket** pkt) {
  GetMockFFmpegImpl()->AVPacketFree(pkt);
}

void mock_av_packet_unref(AVPacket* pkt) {
  GetMockFFmpegImpl()->AVPacketUnref(pkt);
}

int mock_av_read_frame(AVFormatContext* s, AVPacket* pkt) {
  return GetMockFFmpegImpl()->AVReadFrame(s, pkt);
}

AVCodecContext* mock_avcodec_alloc_context3(const AVCodec* codec) {
  return GetMockFFmpegImpl()->AVCodecAllocContext3(codec);
}

int mock_avcodec_parameters_to_context(AVCodecContext* codec,
                                       const AVCodecParameters* par) {
  return GetMockFFmpegImpl()->AVCodecParametersToContext(codec, par);
}

// Returns an FFMPEGDispatch instance that forwards calls to the mock stored in
// GetMockFFmpegImpl() above. The returned FFMPEGDispatch should not be
// deleted; it has static storage duration.
FFMPEGDispatch* GetFFMPEGDispatch() {
  static auto* const ffmpeg_dispatch = []() -> FFMPEGDispatch* {
    auto* dispatch = new FFMPEGDispatch;
    dispatch->avcodec_free_context = &mock_avcodec_free_context;
    dispatch->av_free = &mock_av_free;
    dispatch->av_rescale_rnd = &mock_av_rescale_rnd;
    dispatch->av_dict_get = &mock_av_dict_get;
    dispatch->avformat_open_input = &mock_avformat_open_input;
    dispatch->avformat_close_input = &mock_avformat_close_input;
    dispatch->avio_alloc_context = &mock_avio_alloc_context;
    dispatch->av_malloc = &mock_av_malloc;
    dispatch->avformat_alloc_context = &mock_avformat_alloc_context;
    dispatch->avformat_find_stream_info = &mock_avformat_find_stream_info;
    dispatch->av_seek_frame = &mock_av_seek_frame;
    dispatch->av_packet_alloc = &mock_av_packet_alloc;
    dispatch->av_packet_free = &mock_av_packet_free;
    dispatch->av_packet_unref = &mock_av_packet_unref;
    dispatch->av_read_frame = &mock_av_read_frame;
    dispatch->avcodec_alloc_context3 = &mock_avcodec_alloc_context3;
    dispatch->avcodec_parameters_to_context =
        &mock_avcodec_parameters_to_context;
    return dispatch;
  }();

  return ffmpeg_dispatch;
}

// A mock class representing a data source passed to the cobalt extension
// demuxer.
class MockDataSource {
 public:
  MOCK_METHOD2(BlockingRead, int(uint8_t* data, int bytes_requested));
  MOCK_METHOD1(SeekTo, void(int position));
  MOCK_METHOD0(GetPosition, int64_t());
  MOCK_METHOD0(GetSize, int64_t());
};

// These functions forward calls to a MockDataSource.
int MockBlockingRead(uint8_t* data, int bytes_requested, void* user_data) {
  return static_cast<MockDataSource*>(user_data)->BlockingRead(data,
                                                               bytes_requested);
}

void MockSeekTo(int position, void* user_data) {
  static_cast<MockDataSource*>(user_data)->SeekTo(position);
}

int64_t MockGetPosition(void* user_data) {
  return static_cast<MockDataSource*>(user_data)->GetPosition();
}

int64_t MockGetSize(void* user_data) {
  return static_cast<MockDataSource*>(user_data)->GetSize();
}

// A test fixture is used to ensure that the (static) mock is checked and reset
// between tests.
class FFmpegDemuxerTest : public ::testing::Test {
 public:
  FFmpegDemuxerTest() {
    FFmpegDemuxer::TestOnlySetFFmpegDispatch(GetFFMPEGDispatch());
  }

  ~FFmpegDemuxerTest() override {
    testing::Mock::VerifyAndClearExpectations(GetMockFFmpegImpl());
  }
};

TEST_F(FFmpegDemuxerTest, InitializeAllocatesContextAndOpensInput) {
  auto* const mock_ffmpeg_wrapper = GetMockFFmpegImpl();

  AVFormatContext format_context = {};
  AVInputFormat iformat = {};
  iformat.name = "mp4";
  format_context.iformat = &iformat;

  std::vector<AVStream> streams = {AVStream{}};
  std::vector<AVStream*> stream_ptrs = {&streams[0]};
  std::vector<AVCodecParameters> stream_params = {AVCodecParameters{}};
  stream_params[0].codec_type = AVMEDIA_TYPE_AUDIO;
  stream_params[0].codec_id = AV_CODEC_ID_AAC;

  AVIOContext avio_context = {};
  AVCodecContext codec_context = {};

  // Sanity checks; if any of these fail, the test has a bug.
  SB_CHECK(streams.size() == stream_ptrs.size());
  SB_CHECK(streams.size() == stream_params.size());

  for (int i = 0; i < streams.size(); ++i) {
    streams[i].codecpar = &stream_params[i];
    streams[i].time_base.num = 1;
    streams[i].time_base.den = 1000000;
    streams[i].start_time = 0;
  }

  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatAllocContext())
      .WillOnce(Return(&format_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatCloseInput(Pointee(Eq(&format_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVMalloc(_)).Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVIOAllocContext(_, _, _, _, _, _, _))
      .WillOnce(Return(&avio_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatOpenInput(Pointee(Eq(&format_context)), _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatFindStreamInfo(&format_context, _))
      .WillOnce(WithArg<0>(Invoke([&stream_ptrs](AVFormatContext* context) {
        context->nb_streams = stream_ptrs.size();
        context->streams = stream_ptrs.data();
        context->duration = 120 * AV_TIME_BASE;
        return 0;
      })));
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVCodecAllocContext3(_))
      .WillOnce(Return(&codec_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecFreeContext(Pointee(Eq(&codec_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecParametersToContext(&codec_context, streams[0].codecpar))
      .WillOnce(WithArg<0>(Invoke([](AVCodecContext* context) {
        context->codec_id = AV_CODEC_ID_AAC;
        context->sample_fmt = AV_SAMPLE_FMT_FLT;
        context->channel_layout = AV_CH_LAYOUT_STEREO;
        context->channels = 2;
        context->sample_rate = 44100;
        return 0;
      })));

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  MockDataSource data_source;
  CobaltExtensionDemuxerDataSource c_data_source{
      &MockBlockingRead, &MockSeekTo,  &MockGetPosition,
      &MockGetSize,      kIsStreaming, &data_source};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(api, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  api->DestroyDemuxer(demuxer);
}

TEST_F(FFmpegDemuxerTest, ReadsDataFromDataSource) {
  auto* const mock_ffmpeg_wrapper = GetMockFFmpegImpl();
  constexpr size_t kReadSize = 5;

  AVFormatContext format_context = {};
  AVInputFormat iformat = {};
  iformat.name = "mp4";
  format_context.iformat = &iformat;

  std::vector<AVStream> streams = {AVStream{}};
  std::vector<AVStream*> stream_ptrs = {&streams[0]};
  std::vector<AVCodecParameters> stream_params = {AVCodecParameters{}};
  stream_params[0].codec_type = AVMEDIA_TYPE_AUDIO;
  stream_params[0].codec_id = AV_CODEC_ID_AAC;

  AVIOContext avio_context = {};
  AVCodecContext codec_context = {};

  // Sanity checks; if any of these fail, the test has a bug.
  SB_CHECK(streams.size() == stream_ptrs.size());
  SB_CHECK(streams.size() == stream_params.size());

  for (int i = 0; i < streams.size(); ++i) {
    streams[i].codecpar = &stream_params[i];
    streams[i].time_base.num = 1;
    streams[i].time_base.den = 1000000;
    streams[i].start_time = 0;
    streams[i].index = i;
  }

  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatAllocContext())
      .WillOnce(Return(&format_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatCloseInput(Pointee(Eq(&format_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVMalloc(_)).Times(1);

  // We will capture the AVIO read operation passed to FFmpeg, so that we can
  // simulate FFmpeg reading data from the data source.
  int (*read_packet)(void*, uint8_t*, int) = nullptr;
  // Data blob that will be passed to read_packet.
  void* opaque_read_packet = nullptr;
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVIOAllocContext(_, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&opaque_read_packet), SaveArg<4>(&read_packet),
                      Return(&avio_context)));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatOpenInput(Pointee(Eq(&format_context)), _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatFindStreamInfo(&format_context, _))
      .WillOnce(WithArg<0>(Invoke([&stream_ptrs](AVFormatContext* context) {
        context->nb_streams = stream_ptrs.size();
        context->streams = stream_ptrs.data();
        context->duration = 120 * AV_TIME_BASE;
        return 0;
      })));
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVCodecAllocContext3(_))
      .WillOnce(Return(&codec_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecFreeContext(Pointee(Eq(&codec_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecParametersToContext(&codec_context, streams[0].codecpar))
      .WillOnce(WithArg<0>(Invoke([](AVCodecContext* context) {
        context->codec_id = AV_CODEC_ID_AAC;
        context->sample_fmt = AV_SAMPLE_FMT_FLT;
        context->channel_layout = AV_CH_LAYOUT_STEREO;
        context->channels = 2;
        context->sample_rate = 44100;
        return 0;
      })));
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVReadFrame(&format_context, _))
      .WillOnce(WithArg<1>(Invoke([&opaque_read_packet, &read_packet,
                                   kReadSize](AVPacket* packet) {
        SB_CHECK(read_packet != nullptr)
            << "FFmpeg's read operation should be set via avio_alloc_context "
               "before av_read_frame is called.";
        // This will be freed when av_packet_free is called (which eventually
        // calls AVPacketFree).
        packet->data =
            static_cast<uint8_t*>(malloc(kReadSize * sizeof(uint8_t)));
        packet->size = kReadSize;
        read_packet(opaque_read_packet, packet->data, kReadSize);
        return 0;
      })));

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());

  std::vector<uint8_t> expected_data = {0, 1, 2, 3, 4};
  SB_CHECK(expected_data.size() == kReadSize);

  MockDataSource data_source;
  EXPECT_CALL(data_source, BlockingRead(_, kReadSize))
      .WillOnce(WithArg<0>(Invoke([expected_data](uint8_t* buffer) {
        for (int i = 0; i < expected_data.size(); ++i) {
          buffer[i] = expected_data[i];
        }
        return kReadSize;
      })));
  CobaltExtensionDemuxerDataSource c_data_source{
      &MockBlockingRead, &MockSeekTo,  &MockGetPosition,
      &MockGetSize,      kIsStreaming, &data_source};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  const CobaltExtensionDemuxerBuffer expected_buffer = {
      expected_data.data(),
      static_cast<int64_t>(expected_data.size()),
      nullptr,
      0,
      0,
      0,
      false,
      false};

  MockFunction<void(CobaltExtensionDemuxerBuffer*)> read_cb;
  AVPacket av_packet = {};

  // This is the main check: we ensure that the expected buffer is passed to us
  // via the read callback.
  EXPECT_CALL(read_cb, Call(Pointee(BufferMatches(expected_buffer)))).Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVPacketAlloc())
      .WillOnce(Return(&av_packet));
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVPacketFree(Pointee(Eq(&av_packet))))
      .WillOnce(Invoke([](AVPacket** av_packet) { free((*av_packet)->data); }));

  demuxer->Read(kCobaltExtensionDemuxerStreamTypeAudio,
                &CallMockCB<decltype(read_cb), CobaltExtensionDemuxerBuffer>,
                &read_cb, demuxer->user_data);

  api->DestroyDemuxer(demuxer);
}

TEST_F(FFmpegDemuxerTest, ReturnsAudioConfig) {
  auto* const mock_ffmpeg_wrapper = GetMockFFmpegImpl();

  AVFormatContext format_context = {};
  AVInputFormat iformat = {};
  iformat.name = "mp4";
  format_context.iformat = &iformat;

  std::vector<AVStream> streams = {AVStream{}};
  std::vector<AVStream*> stream_ptrs = {&streams[0]};
  std::vector<AVCodecParameters> stream_params = {AVCodecParameters{}};
  stream_params[0].codec_type = AVMEDIA_TYPE_AUDIO;
  stream_params[0].codec_id = AV_CODEC_ID_AAC;

  AVIOContext avio_context = {};
  AVCodecContext codec_context = {};

  // Sanity checks; if any of these fail, the test has a bug.
  SB_CHECK(streams.size() == stream_ptrs.size());
  SB_CHECK(streams.size() == stream_params.size());

  for (int i = 0; i < streams.size(); ++i) {
    streams[i].codecpar = &stream_params[i];
    streams[i].time_base.num = 1;
    streams[i].time_base.den = 1000000;
    streams[i].start_time = 0;
  }

  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatAllocContext())
      .WillOnce(Return(&format_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatCloseInput(Pointee(Eq(&format_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVMalloc(_)).Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVIOAllocContext(_, _, _, _, _, _, _))
      .WillOnce(Return(&avio_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatOpenInput(Pointee(Eq(&format_context)), _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatFindStreamInfo(&format_context, _))
      .WillOnce(WithArg<0>(Invoke([&stream_ptrs](AVFormatContext* context) {
        context->nb_streams = stream_ptrs.size();
        context->streams = stream_ptrs.data();
        context->duration = 120 * AV_TIME_BASE;
        return 0;
      })));
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVCodecAllocContext3(_))
      .WillOnce(Return(&codec_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecFreeContext(Pointee(Eq(&codec_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecParametersToContext(&codec_context, streams[0].codecpar))
      .WillOnce(WithArg<0>(Invoke([](AVCodecContext* context) {
        context->codec_id = AV_CODEC_ID_AAC;
        context->sample_fmt = AV_SAMPLE_FMT_FLT;
        context->channel_layout = AV_CH_LAYOUT_STEREO;
        context->channels = 2;
        context->sample_rate = 44100;
        return 0;
      })));

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  MockDataSource data_source;
  CobaltExtensionDemuxerDataSource c_data_source{
      &MockBlockingRead, &MockSeekTo,  &MockGetPosition,
      &MockGetSize,      kIsStreaming, &data_source};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(api, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  CobaltExtensionDemuxerAudioDecoderConfig actual_audio_config = {};
  demuxer->GetAudioConfig(&actual_audio_config, demuxer->user_data);

  // These values are derived from those set via AVCodecParametersToContext.
  EXPECT_EQ(actual_audio_config.codec, kCobaltExtensionDemuxerCodecAAC);
  EXPECT_EQ(actual_audio_config.sample_format,
            kCobaltExtensionDemuxerSampleFormatF32);
  EXPECT_EQ(actual_audio_config.channel_layout,
            kCobaltExtensionDemuxerChannelLayoutStereo);
  EXPECT_EQ(actual_audio_config.encryption_scheme,
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted);
  EXPECT_EQ(actual_audio_config.samples_per_second, 44100);
  EXPECT_THAT(actual_audio_config.extra_data, IsNull());
  EXPECT_EQ(actual_audio_config.extra_data_size, 0);

  api->DestroyDemuxer(demuxer);
}

TEST_F(FFmpegDemuxerTest, ReturnsVideoConfig) {
  auto* const mock_ffmpeg_wrapper = GetMockFFmpegImpl();

  AVFormatContext format_context = {};
  AVInputFormat iformat = {};
  iformat.name = "mp4";
  format_context.iformat = &iformat;

  // In this test we simulate both an audio stream and a video stream being
  // present.
  std::vector<AVStream> streams = {AVStream{}, AVStream{}};
  std::vector<AVStream*> stream_ptrs = {&streams[0], &streams[1]};
  std::vector<AVCodecParameters> stream_params = {AVCodecParameters{},
                                                  AVCodecParameters{}};
  stream_params[0].codec_type = AVMEDIA_TYPE_AUDIO;
  stream_params[0].codec_id = AV_CODEC_ID_AAC;
  stream_params[1].codec_type = AVMEDIA_TYPE_VIDEO;
  stream_params[1].codec_id = AV_CODEC_ID_H264;

  AVIOContext avio_context = {};
  AVCodecContext codec_context_1 = {};
  AVCodecContext codec_context_2 = {};

  // Sanity checks; if any of these fail, the test has a bug.
  SB_CHECK(streams.size() == stream_ptrs.size());
  SB_CHECK(streams.size() == stream_params.size());

  for (int i = 0; i < streams.size(); ++i) {
    streams[i].codecpar = &stream_params[i];
    streams[i].time_base.num = 1;
    streams[i].time_base.den = 1000000;
    streams[i].start_time = 0;
  }

  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatAllocContext())
      .WillOnce(Return(&format_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatCloseInput(Pointee(Eq(&format_context))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVMalloc(_)).Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVIOAllocContext(_, _, _, _, _, _, _))
      .WillOnce(Return(&avio_context));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVFormatOpenInput(Pointee(Eq(&format_context)), _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVFormatFindStreamInfo(&format_context, _))
      .WillOnce(WithArg<0>(Invoke([&stream_ptrs](AVFormatContext* context) {
        context->nb_streams = stream_ptrs.size();
        context->streams = stream_ptrs.data();
        context->duration = 120 * AV_TIME_BASE;
        return 0;
      })));
  EXPECT_CALL(*mock_ffmpeg_wrapper, AVCodecAllocContext3(_))
      .WillOnce(Return(&codec_context_1))
      .WillOnce(Return(&codec_context_2));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecFreeContext(Pointee(Eq(&codec_context_1))))
      .Times(1);
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecFreeContext(Pointee(Eq(&codec_context_2))))
      .Times(1);

  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecParametersToContext(_, streams[0].codecpar))
      .WillOnce(WithArg<0>(Invoke([](AVCodecContext* context) {
        context->codec_id = AV_CODEC_ID_AAC;
        context->sample_fmt = AV_SAMPLE_FMT_FLT;
        context->channel_layout = AV_CH_LAYOUT_STEREO;
        context->channels = 2;
        context->sample_rate = 44100;
        return 0;
      })));
  EXPECT_CALL(*mock_ffmpeg_wrapper,
              AVCodecParametersToContext(_, streams[1].codecpar))
      .WillOnce(WithArg<0>(Invoke([](AVCodecContext* context) {
        context->codec_id = AV_CODEC_ID_H264;
        context->width = 1920;
        context->height = 1080;
        context->sample_aspect_ratio.num = 1;
        context->sample_aspect_ratio.den = 1;
        context->profile = FF_PROFILE_H264_BASELINE;
        context->pix_fmt = AV_PIX_FMT_YUVJ420P;
        context->colorspace = AVCOL_SPC_BT709;
        context->color_range = AVCOL_RANGE_MPEG;
        return 0;
      })));

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  MockDataSource data_source;
  CobaltExtensionDemuxerDataSource c_data_source{
      &MockBlockingRead, &MockSeekTo,  &MockGetPosition,
      &MockGetSize,      kIsStreaming, &data_source};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(api, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  CobaltExtensionDemuxerVideoDecoderConfig actual_video_config = {};
  demuxer->GetVideoConfig(&actual_video_config, demuxer->user_data);

  // These values are derived from those set via AVCodecParametersToContext.
  EXPECT_EQ(actual_video_config.codec, kCobaltExtensionDemuxerCodecH264);
  EXPECT_EQ(actual_video_config.profile,
            kCobaltExtensionDemuxerH264ProfileBaseline);
  EXPECT_EQ(actual_video_config.coded_width, 1920);
  EXPECT_EQ(actual_video_config.coded_height, 1080);
  EXPECT_EQ(actual_video_config.visible_rect_x, 0);
  EXPECT_EQ(actual_video_config.visible_rect_y, 0);
  EXPECT_EQ(actual_video_config.visible_rect_width, 1920);
  EXPECT_EQ(actual_video_config.visible_rect_height, 1080);
  EXPECT_EQ(actual_video_config.natural_width, 1920);
  EXPECT_EQ(actual_video_config.natural_height, 1080);
  EXPECT_THAT(actual_video_config.extra_data, IsNull());
  EXPECT_EQ(actual_video_config.extra_data_size, 0);

  api->DestroyDemuxer(demuxer);
}

}  // namespace
}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
