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

#include "media/base/filters.h"
#include "media/base/filter_collection.h"
#include "media/base/pipeline.h"
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
  MOCK_METHOD1(Play, void(FilterCallback* callback));
  MOCK_METHOD1(Pause, void(FilterCallback* callback));
  MOCK_METHOD1(Flush, void(FilterCallback* callback));
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
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
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // DataSource implementation.
  MOCK_METHOD1(IsUrlSupported, bool(const std::string& url));
  MOCK_METHOD2(Initialize, void(const std::string& url,
                                FilterCallback* callback));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD4(Read, void(int64 position, size_t size, uint8* data,
                          DataSource::ReadCallback* callback));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD0(IsStreaming, bool());

 protected:
  virtual ~MockDataSource();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // Demuxer implementation.
  MOCK_METHOD2(Initialize, void(DataSource* data_source,
                                FilterCallback* callback));
  MOCK_METHOD0(GetNumberOfStreams, size_t());
  MOCK_METHOD1(GetStream, scoped_refptr<DemuxerStream>(int stream_id));

 protected:
  virtual ~MockDemuxer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream();

  // DemuxerStream implementation.
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(Read, void(Callback1<Buffer*>::Type* read_callback));
  MOCK_METHOD1(QueryInterface, void*(const char* interface_id));
  MOCK_METHOD0(EnableBitstreamConverter, void());

 protected:
  virtual ~MockDemuxerStream();

 private:
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // VideoDecoder implementation.
  MOCK_METHOD3(Initialize, void(DemuxerStream* stream,
                                FilterCallback* callback,
                                StatisticsCallback* stats_callback));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(ProduceVideoFrame, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD0(ProvidesBuffer, bool());

  // Make this method public so that tests can make use of it.
  void VideoFrameReady(scoped_refptr<VideoFrame> frame) {
    VideoDecoder::VideoFrameReady(frame);
  }

 protected:
  virtual ~MockVideoDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();

  // Filter implementation.
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // AudioDecoder implementation.
  MOCK_METHOD3(Initialize, void(DemuxerStream* stream,
                                FilterCallback* callback,
                                StatisticsCallback* stats_callback));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(ProduceAudioSamples, void(scoped_refptr<Buffer>));

  // change to public to allow unittest for access;
  ConsumeAudioSamplesCallback* consume_audio_samples_callback() {
    return AudioDecoder::consume_audio_samples_callback();
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
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // VideoRenderer implementation.
  MOCK_METHOD3(Initialize, void(VideoDecoder* decoder,
                                FilterCallback* callback,
                                StatisticsCallback* stats_callback));
  MOCK_METHOD0(HasEnded, bool());
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
  MOCK_METHOD1(Stop, void(FilterCallback* callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, FilterCallback* callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());

  // AudioRenderer implementation.
  MOCK_METHOD2(Initialize, void(AudioDecoder* decoder,
                                FilterCallback* callback));
  MOCK_METHOD0(HasEnded, bool());
  MOCK_METHOD1(SetVolume, void(float volume));

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
  MockDataSource* data_source() const { return data_source_; }
  MockDemuxer* demuxer() const { return demuxer_; }
  MockVideoDecoder* video_decoder() const { return video_decoder_; }
  MockAudioDecoder* audio_decoder() const { return audio_decoder_; }
  MockVideoRenderer* video_renderer() const { return video_renderer_; }
  MockAudioRenderer* audio_renderer() const { return audio_renderer_; }

  FilterCollection* filter_collection() const {
    return filter_collection(true);
  }

  FilterCollection* filter_collection(bool include_data_source) const;

 private:
  scoped_refptr<MockDataSource> data_source_;
  scoped_refptr<MockDemuxer> demuxer_;
  scoped_refptr<MockVideoDecoder> video_decoder_;
  scoped_refptr<MockAudioDecoder> audio_decoder_;
  scoped_refptr<MockVideoRenderer> video_renderer_;
  scoped_refptr<MockAudioRenderer> audio_renderer_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterCollection);
};

// Helper gmock functions that immediately executes and destroys the
// FilterCallback on behalf of the provided filter.  Can be used when mocking
// the Initialize() and Seek() methods.
void RunFilterCallback(::testing::Unused, FilterCallback* callback);
void RunFilterCallback3(::testing::Unused, FilterCallback* callback,
                        ::testing::Unused);

// Helper gmock function that immediately destroys the FilterCallback on behalf
// of the provided filter.  Can be used when mocking the Initialize() and Seek()
// methods.
void DestroyFilterCallback(::testing::Unused, FilterCallback* callback);

// Helper gmock function that immediately executes and destroys the
// FilterCallback on behalf of the provided filter.  Can be used when mocking
// the Stop() method.
void RunStopFilterCallback(FilterCallback* callback);

// Helper gmock action that calls SetError() on behalf of the provided filter.
ACTION_P2(SetError, filter, error) {
  filter->host()->SetError(error);
}

// Helper gmock action that calls SetDuration() on behalf of the provided
// filter.
ACTION_P2(SetDuration, filter, duration) {
  filter->host()->SetDuration(duration);
}

// Helper gmock action that calls SetTotalBytes() on behalf of the provided
// filter.
ACTION_P2(SetTotalBytes, filter, bytes) {
  filter->host()->SetTotalBytes(bytes);
}

// Helper gmock action that calls SetBufferedBytes() on behalf of the provided
// filter.
ACTION_P2(SetBufferedBytes, filter, bytes) {
  filter->host()->SetBufferedBytes(bytes);
}

// Helper gmock action that calls DisableAudioRenderer() on behalf of the
// provided filter.
ACTION_P(DisableAudioRenderer, filter) {
  filter->host()->DisableAudioRenderer();
}

// Helper mock statistics callback.
class MockStatisticsCallback {
 public:
  MOCK_METHOD1(OnStatistics, void(const media::PipelineStatistics& statistics));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
