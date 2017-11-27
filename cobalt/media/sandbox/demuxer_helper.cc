// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/media/sandbox/demuxer_helper.h"

#include <queue>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "cobalt/media/fetcher_buffered_data_source.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/shell_demuxer.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {

using ::media::AudioDecoderConfig;
using ::media::BindToCurrentLoop;
using ::media::DecoderBuffer;
using ::media::DemuxerStream;
using ::media::VideoDecoderConfig;

// A DemuxerStream that caches media data in memory.  Note that it is only for
// test purpose and no synchronization is provided.
class DemuxerStreamCache : public ::media::DemuxerStream {
 public:
  explicit DemuxerStreamCache(const AudioDecoderConfig& audio_decoder_config)
      : type_(AUDIO) {
    audio_decoder_config_.CopyFrom(audio_decoder_config);
  }
  explicit DemuxerStreamCache(const VideoDecoderConfig& video_decoder_config)
      : type_(VIDEO) {
    video_decoder_config_.CopyFrom(video_decoder_config);
  }

  void Append(const scoped_refptr<DecoderBuffer>& buffer) {
    DCHECK(buffer);
    DCHECK(!buffer->IsEndOfStream());
    buffers_.push(buffer);
  }

 private:
  // DemuxerStream methods.
  void Read(const ReadCB& read_cb) override {
    scoped_refptr<DecoderBuffer> buffer;
    if (buffers_.empty()) {
      buffer = DecoderBuffer::CreateEOSBuffer(base::TimeDelta());
    } else {
      buffer = buffers_.front();
      buffers_.pop();
    }
    read_cb.Run(kOk, buffer);
  }
  const AudioDecoderConfig& audio_decoder_config() override {
    DCHECK_EQ(type_, AUDIO);
    return audio_decoder_config_;
  }
  const VideoDecoderConfig& video_decoder_config() override {
    DCHECK_EQ(type_, VIDEO);
    return video_decoder_config_;
  }
  Type type() override { return type_; }
  void EnableBitstreamConverter() override {}
  bool StreamWasEncrypted() const override { return false; }

  Type type_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
  std::queue<scoped_refptr<DecoderBuffer> > buffers_;
};

// A Demuxer that caches media data in memory.  Note that it is only for test
// purpose and no synchronization is provided.
class DemuxerCache : public ::media::Demuxer {
 public:
  DemuxerCache(const scoped_refptr<Demuxer>& demuxer, uint64 bytes_to_cache,
               const DemuxerHelper::DemuxerReadyCB& demuxer_ready_cb) {
    if (demuxer->GetStream(DemuxerStream::AUDIO)) {
      audio_stream_ = new DemuxerStreamCache(
          demuxer->GetStream(DemuxerStream::AUDIO)->audio_decoder_config());
    }
    if (demuxer->GetStream(DemuxerStream::VIDEO)) {
      video_stream_ = new DemuxerStreamCache(
          demuxer->GetStream(DemuxerStream::VIDEO)->video_decoder_config());
    }
    DCHECK(audio_stream_ || video_stream_);
    CacheState cache_state;
    cache_state.demuxer = demuxer;
    cache_state.bytes_to_cache = bytes_to_cache;
    cache_state.audio_eos = !audio_stream_;
    cache_state.video_eos = !video_stream_;
    cache_state.demuxer_ready_cb = demuxer_ready_cb;

    if (audio_stream_) {
      cache_state.last_read_type = DemuxerStream::AUDIO;
    } else {
      cache_state.last_read_type = DemuxerStream::VIDEO;
    }
    demuxer->GetStream(cache_state.last_read_type)
        ->Read(BindToCurrentLoop(
            base::Bind(&DemuxerCache::CacheBuffer, this, cache_state)));
  }

 private:
  struct CacheState {
    scoped_refptr<Demuxer> demuxer;
    uint64 bytes_to_cache;
    base::TimeDelta last_audio_buffer_timestamp;
    bool audio_eos;
    base::TimeDelta last_video_buffer_timestamp;
    bool video_eos;
    DemuxerHelper::DemuxerReadyCB demuxer_ready_cb;
    DemuxerStream::Type last_read_type;
  };

  void CacheBuffer(CacheState cache_state, DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer) {
    DCHECK_EQ(status, DemuxerStream::kOk);
    DCHECK(buffer);

    if (cache_state.last_read_type == DemuxerStream::AUDIO) {
      if (buffer->IsEndOfStream()) {
        cache_state.audio_eos = true;
      } else {
        cache_state.last_audio_buffer_timestamp = buffer->GetTimestamp();
        if (cache_state.bytes_to_cache >= buffer->GetDataSize()) {
          cache_state.bytes_to_cache -= buffer->GetDataSize();
          audio_stream_->Append(buffer);
        } else {
          cache_state.bytes_to_cache = 0;
        }
      }
    } else {
      DCHECK_EQ(cache_state.last_read_type, DemuxerStream::VIDEO);
      if (buffer->IsEndOfStream()) {
        cache_state.video_eos = true;
      } else {
        cache_state.last_video_buffer_timestamp = buffer->GetTimestamp();
        if (cache_state.bytes_to_cache >= buffer->GetDataSize()) {
          cache_state.bytes_to_cache -= buffer->GetDataSize();
          video_stream_->Append(buffer);
        } else {
          cache_state.bytes_to_cache = 0;
        }
      }
    }
    if ((cache_state.audio_eos && cache_state.video_eos) ||
        cache_state.bytes_to_cache == 0) {
      cache_state.demuxer->Stop(base::Bind(cache_state.demuxer_ready_cb,
                                           scoped_refptr<Demuxer>(this)));
      return;
    }
    cache_state.last_read_type = DemuxerStream::AUDIO;
    if (cache_state.audio_eos) {
      cache_state.last_read_type = DemuxerStream::VIDEO;
    } else if (!cache_state.video_eos) {
      if (cache_state.last_video_buffer_timestamp <
          cache_state.last_audio_buffer_timestamp) {
        cache_state.last_read_type = DemuxerStream::VIDEO;
      }
    }

    cache_state.demuxer->GetStream(cache_state.last_read_type)
        ->Read(BindToCurrentLoop(
            base::Bind(&DemuxerCache::CacheBuffer, this, cache_state)));
  }

  // Demuxer methods.
  void Initialize(::media::DemuxerHost* host,
                  const ::media::PipelineStatusCB& status_cb) override {
    NOTREACHED();
  }
  void Stop(const base::Closure& callback) override {
    DCHECK(!callback.is_null());
    callback.Run();
  }
  scoped_refptr<DemuxerStream> GetStream(DemuxerStream::Type type) override {
    if (type == DemuxerStream::AUDIO) {
      return audio_stream_;
    } else if (type == DemuxerStream::VIDEO) {
      return video_stream_;
    }
    NOTREACHED();
    return NULL;
  }
  base::TimeDelta GetStartTime() const override { return base::TimeDelta(); }

  scoped_refptr<DemuxerStreamCache> audio_stream_;
  scoped_refptr<DemuxerStreamCache> video_stream_;
};

}  // namespace

class DemuxerHelper::DemuxerHostStub : public ::media::DemuxerHost {
 private:
  // DataSourceHost methods
  void SetTotalBytes(int64 total_bytes) override {
    UNREFERENCED_PARAMETER(total_bytes);
  }
  void AddBufferedByteRange(int64 start, int64 end) override {
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
  }
  void AddBufferedTimeRange(base::TimeDelta start,
                            base::TimeDelta end) override {
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
  }

  // DemuxerHost methods
  void SetDuration(base::TimeDelta duration) override {
    UNREFERENCED_PARAMETER(duration);
  }
  void OnDemuxerError(::media::PipelineStatus error) override {
    UNREFERENCED_PARAMETER(error);
  }
};

DemuxerHelper::DemuxerHelper(
    const scoped_refptr<base::MessageLoopProxy>& media_message_loop,
    loader::FetcherFactory* fetcher_factory, const GURL& video_url,
    const DemuxerReadyCB& demuxer_ready_cb)
    : host_(new DemuxerHostStub) {
  CreateDemuxer(media_message_loop, fetcher_factory, video_url,
                demuxer_ready_cb, 0);
}

DemuxerHelper::DemuxerHelper(
    const scoped_refptr<base::MessageLoopProxy>& media_message_loop,
    loader::FetcherFactory* fetcher_factory, const GURL& video_url,
    const DemuxerReadyCB& demuxer_ready_cb, uint64 bytes_to_cache)
    : host_(new DemuxerHostStub) {
  DCHECK_GE(bytes_to_cache, 0);

  CreateDemuxer(media_message_loop, fetcher_factory, video_url,
                demuxer_ready_cb, bytes_to_cache);
}

DemuxerHelper::~DemuxerHelper() { delete host_; }

void DemuxerHelper::CreateDemuxer(
    const scoped_refptr<base::MessageLoopProxy>& media_message_loop,
    loader::FetcherFactory* fetcher_factory, const GURL& video_url,
    const DemuxerReadyCB& demuxer_ready_cb, uint64 bytes_to_cache) {
  DCHECK(media_message_loop);
  DCHECK(fetcher_factory);
  DCHECK(!demuxer_ready_cb.is_null());

  scoped_refptr<FetcherBufferedDataSource> data_source(
      new FetcherBufferedDataSource(media_message_loop, video_url,
                                    csp::SecurityCallback(),
                                    fetcher_factory->network_module(),
                                    loader::kNoCORSMode, loader::Origin()));
  scoped_refptr<Demuxer> demuxer =
      new ::media::ShellDemuxer(media_message_loop, data_source);
  demuxer->Initialize(
      host_, base::Bind(&DemuxerHelper::OnDemuxerReady, base::Unretained(this),
                        demuxer, demuxer_ready_cb, bytes_to_cache));
}

void DemuxerHelper::OnDemuxerReady(const scoped_refptr<Demuxer>& demuxer,
                                   const DemuxerReadyCB& demuxer_ready_cb,
                                   uint64 bytes_to_cache,
                                   ::media::PipelineStatus status) {
  DCHECK_EQ(status, ::media::PIPELINE_OK);

  if (bytes_to_cache == 0) {
    demuxer_ready_cb.Run(demuxer);
    return;
  }

  // The newly created object is reference counted and will be passed back
  // through demuxer_ready_cb.
  new DemuxerCache(demuxer, bytes_to_cache, demuxer_ready_cb);
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
