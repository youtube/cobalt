// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SANDBOX_DEMUXER_HELPER_H_
#define COBALT_MEDIA_SANDBOX_DEMUXER_HELPER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/loader/fetcher_factory.h"
#include "media/base/demuxer.h"
#include "url/gurl.h"

namespace cobalt {
namespace media {
namespace sandbox {

// This class create and initialize a Demuxer and then call the callback
// provided.  It can also cache media data in memory to avoid any I/O during
// playback.
class DemuxerHelper {
 public:
  typedef ::media::Demuxer Demuxer;
  typedef base::Callback<void(const scoped_refptr<Demuxer>&)> DemuxerReadyCB;

  DemuxerHelper(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_message_loop,
      loader::FetcherFactory* fetcher_factory, const GURL& video_url,
      const DemuxerReadyCB& demuxer_ready_cb);
  // This ctor will create an Demuxer internally that caches the media data in
  // memory before calling the callback.  This ensures that there is no I/O
  // incurred during playback.
  DemuxerHelper(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_message_loop,
      loader::FetcherFactory* fetcher_factory, const GURL& video_url,
      const DemuxerReadyCB& demuxer_ready_cb, uint64 bytes_to_cache);
  ~DemuxerHelper();

 private:
  void CreateDemuxer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_message_loop,
      loader::FetcherFactory* fetcher_factory, const GURL& video_url,
      const DemuxerReadyCB& demuxer_ready_cb, uint64 bytes_to_cache);
  void OnDemuxerReady(const scoped_refptr<Demuxer>& demuxer,
                      const DemuxerReadyCB& ready_cb, uint64 bytes_to_cache,
                      ::media::PipelineStatus status);

  class DemuxerHostStub;
  DemuxerHostStub* host_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DemuxerHelper);
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_DEMUXER_HELPER_H_
