// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A new breed of mock media filters, this time using gmock!  Feel free to add
// actions if you need interesting side-effects (i.e., copying data to the
// buffer passed into MockDataSource::Read()).
//
// Don't forget you can use StrictMock<> and NiceMock<> if you want the mock
// filters to fail the test or do nothing when an unexpected method is called.
// http://code.google.com/p/googlemock/wiki/CookBook#Nice_Mocks_and_Strict_Mocks

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <string>

#include "base/callback.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/filters.h"
#include "media/base/filter_collection.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Use this template to test for object destruction by setting expectations on
// the method OnDestroy().
//
// TODO(scherkus): not sure about the naming...  perhaps contribute this back
// to gmock itself!
template<class MockClass>
class Destroyable : public MockClass {
 public:
  Destroyable() {}

  MOCK_METHOD0(OnDestroy, void());

 protected:
  virtual ~Destroyable() {
    OnDestroy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Destroyable);
};

class MockFilter : public Filter {
 public:
  MockFilter();

  // Filter implementation.
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Pause, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

 protected:
  virtual ~MockFilter();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFilter);
};

class MockDataSource : public DataSource {
 public:
  MockDataSource();

  // Filter implementation.
  virtual void set_host(FilterHost* host);

  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // DataSource implementation.
  MOCK_METHOD4(Read, void(int64 position, size_t size, uint8* data,
                          const DataSource::ReadCallback& callback));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD1(SetPreload, void(Preload preload));
  MOCK_METHOD1(SetBitrate, void(int bitrate));
  MOCK_METHOD0(IsStreaming, bool());

  // Sets the TotalBytes & BufferedBytes values to be sent to host() when
  // the set_host() is called.
  void SetTotalAndBufferedBytes(int64 total_bytes, int64 buffered_bytes);

 protected:
  virtual ~MockDataSource();

 private:
  int64 total_bytes_;
  int64 buffered_bytes_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();
  // Filter implementation.
  virtual void set_host(FilterHost* host);
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(SetPreload, void(Preload preload));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());
  MOCK_METHOD0(GetBitrate, int());

  // Demuxer implementation.
  MOCK_METHOD2(Initialize, void(DataSource* data_source,
                                const base::Closure& callback));
  MOCK_METHOD1(GetStream, scoped_refptr<DemuxerStream>(DemuxerStream::Type));
  MOCK_CONST_METHOD0(GetStartTime, base::TimeDelta());

  // Sets the TotalBytes, BufferedBytes, & Duration values to be sent to host()
  // when set_host() is called.
  void SetTotalAndBufferedBytesAndDuration(
      int64 total_bytes, int64 buffered_bytes, const base::TimeDelta& duration);

 protected:
  virtual ~MockDemuxer();

 private:
  int64 total_bytes_;
  int64 buffered_bytes_;
  base::TimeDelta duration_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerFactory : public DemuxerFactory {
 public:
  explicit MockDemuxerFactory(MockDemuxer* demuxer);
  virtual ~MockDemuxerFactory();

  void SetError(PipelineStatus error);
  void RunBuildCallback(const std::string& url, const BuildCallback& callback);

  // DemuxerFactory methods.
  MOCK_METHOD2(Build, void(const std::string& url,
                           const BuildCallback& callback));
  virtual DemuxerFactory* Clone() const;

 private:
  scoped_refptr<MockDemuxer> demuxer_;
  PipelineStatus status_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerFactory);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream();

  // DemuxerStream implementation.
  MOCK_METHOD0(type, Type());
  MOCK_METHOD1(Read, void(const ReadCallback& read_callback));
  MOCK_METHOD0(audio_decoder_config, const AudioDecoderConfig&());
  MOCK_METHOD0(video_decoder_config, const VideoDecoderConfig&());
  MOCK_METHOD0(EnableBitstreamConverter, void());

 protected:
  virtual ~MockDemuxerStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // VideoDecoder implementation.
  MOCK_METHOD3(Initialize, void(DemuxerStream* stream,
                                const base::Closure& callback,
                                const StatisticsCallback& stats_callback));
  MOCK_METHOD1(Read, void(const ReadCB& callback));
  MOCK_METHOD0(natural_size, const gfx::Size&());

 protected:
  virtual ~MockVideoDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // AudioDecoder implementation.
  MOCK_METHOD3(Initialize, void(DemuxerStream* stream,
                                const base::Closure& callback,
                                const StatisticsCallback& stats_callback));
  MOCK_METHOD1(ProduceAudioSamples, void(scoped_refptr<Buffer>));
  MOCK_METHOD0(bits_per_channel, int(void));
  MOCK_METHOD0(channel_layout, ChannelLayout(void));
  MOCK_METHOD0(samples_per_second, int(void));

  void ConsumeAudioSamplesForTest(scoped_refptr<Buffer> buffer) {
    AudioDecoder::ConsumeAudioSamples(buffer);
  }

 protected:
  virtual ~MockAudioDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // VideoRenderer implementation.
  MOCK_METHOD3(Initialize, void(VideoDecoder* decoder,
                                const base::Closure& callback,
                                const StatisticsCallback& stats_callback));
  MOCK_METHOD0(HasEnded, bool());

  // TODO(scherkus): although VideoRendererBase defines this method, this really
  // shouldn't be here OR should be renamed.
  MOCK_METHOD1(ConsumeVideoFrame, void(scoped_refptr<VideoFrame> frame));

 protected:
  virtual ~MockVideoRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const FilterStatusCB& cb));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // AudioRenderer implementation.
  MOCK_METHOD3(Initialize, void(AudioDecoder* decoder,
                                const base::Closure& init_callback,
                                const base::Closure& underflow_callback));
  MOCK_METHOD0(HasEnded, bool());
  MOCK_METHOD1(SetVolume, void(float volume));

  MOCK_METHOD1(ResumeAfterUnderflow, void(bool buffer_more_audio));

 protected:
  virtual ~MockAudioRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

// FilterFactory that returns canned instances of mock filters.  You can set
// expectations on the filters and then pass the collection into a pipeline.
class MockFilterCollection {
 public:
  MockFilterCollection();
  virtual ~MockFilterCollection();

  // Mock accessors.
  MockDemuxer* demuxer() const { return demuxer_; }
  MockVideoDecoder* video_decoder() const { return video_decoder_; }
  MockAudioDecoder* audio_decoder() const { return audio_decoder_; }
  MockVideoRenderer* video_renderer() const { return video_renderer_; }
  MockAudioRenderer* audio_renderer() const { return audio_renderer_; }

  FilterCollection* filter_collection() const {
    return filter_collection(true, true, true, PIPELINE_OK);
  }

  FilterCollection* filter_collection(bool include_demuxer,
                                      bool run_build_callback,
                                      bool run_build,
                                      PipelineStatus build_status) const;

 private:
  scoped_refptr<MockDemuxer> demuxer_;
  scoped_refptr<MockVideoDecoder> video_decoder_;
  scoped_refptr<MockAudioDecoder> audio_decoder_;
  scoped_refptr<MockVideoRenderer> video_renderer_;
  scoped_refptr<MockAudioRenderer> audio_renderer_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterCollection);
};

// Helper gmock functions that immediately executes and destroys the
// Closure on behalf of the provided filter.  Can be used when mocking
// the Initialize() and Seek() methods.
void RunFilterCallback(::testing::Unused, const base::Closure& callback);
void RunFilterStatusCB(::testing::Unused, const FilterStatusCB& cb);
void RunPipelineStatusCB(PipelineStatus status, const PipelineStatusCB& cb);
void RunFilterCallback3(::testing::Unused, const base::Closure& callback,
                        ::testing::Unused);

// Helper gmock function that immediately executes the Closure on behalf of the
// provided filter.  Can be used when mocking the Stop() method.
void RunStopFilterCallback(const base::Closure& callback);

// Helper gmock action that calls SetError() on behalf of the provided filter.
ACTION_P2(SetError, filter, error) {
  filter->host()->SetError(error);
}

// Helper gmock action that calls SetDuration() on behalf of the provided
// filter.
ACTION_P2(SetDuration, filter, duration) {
  filter->host()->SetDuration(duration);
}

// Helper gmock action that calls DisableAudioRenderer() on behalf of the
// provided filter.
ACTION_P(DisableAudioRenderer, filter) {
  filter->host()->DisableAudioRenderer();
}

// Helper mock statistics callback.
class MockStatisticsCallback {
 public:
  MockStatisticsCallback();
  ~MockStatisticsCallback();

  MOCK_METHOD1(OnStatistics, void(const media::PipelineStatistics& statistics));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
