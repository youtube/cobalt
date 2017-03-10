// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_FAKE_TEXT_TRACK_STREAM_H_
#define COBALT_MEDIA_BASE_FAKE_TEXT_TRACK_STREAM_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace media {

// Fake implementation of the DemuxerStream.  These are the stream objects
// we pass to the text renderer object when streams are added and removed.
class FakeTextTrackStream : public DemuxerStream {
 public:
  FakeTextTrackStream();
  ~FakeTextTrackStream() OVERRIDE;

  // DemuxerStream implementation.
  void Read(const ReadCB&) OVERRIDE;
  MOCK_METHOD0(audio_decoder_config, AudioDecoderConfig());
  MOCK_METHOD0(video_decoder_config, VideoDecoderConfig());
  Type type() const OVERRIDE;
  MOCK_METHOD0(EnableBitstreamConverter, void());
  bool SupportsConfigChanges() OVERRIDE;
  VideoRotation video_rotation() OVERRIDE;
  bool enabled() const OVERRIDE;
  void set_enabled(bool enabled, base::TimeDelta timestamp) OVERRIDE;
  void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) OVERRIDE;

  void SatisfyPendingRead(const base::TimeDelta& start,
                          const base::TimeDelta& duration,
                          const std::string& id, const std::string& content,
                          const std::string& settings);
  void AbortPendingRead();
  void SendEosNotification();

  void Stop();

  MOCK_METHOD0(OnRead, void());

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ReadCB read_cb_;
  bool stopping_;

  DISALLOW_COPY_AND_ASSIGN(FakeTextTrackStream);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_FAKE_TEXT_TRACK_STREAM_H_
