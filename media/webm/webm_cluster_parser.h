// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_
#define MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_

#include <deque>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/stream_parser_buffer.h"
#include "media/webm/webm_parser.h"

namespace media {

class MEDIA_EXPORT WebMClusterParser : public WebMParserClient {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  WebMClusterParser(int64 timecode_scale,
                    int audio_track_num,
                    int video_track_num,
                    const std::string& audio_encryption_key_id,
                    const std::string& video_encryption_key_id,
                    const LogCB& log_cb);
  virtual ~WebMClusterParser();

  // Resets the parser state so it can accept a new cluster.
  void Reset();

  // Parses a WebM cluster element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  base::TimeDelta cluster_start_time() const { return cluster_start_time_; }
  const BufferQueue& audio_buffers() const { return audio_.buffers(); }
  const BufferQueue& video_buffers() const { return video_.buffers(); }

  // Returns true if the last Parse() call stopped at the end of a cluster.
  bool cluster_ended() const { return cluster_ended_; }

 private:
  // Helper class that manages per-track state.
  class Track {
   public:
    explicit Track(int track_num);
    ~Track();

    int track_num() const { return track_num_; }
    const BufferQueue& buffers() const { return buffers_; }

    bool AddBuffer(const scoped_refptr<StreamParserBuffer>& buffer);

    // Clears all buffer state.
    void Reset();

   private:
    int track_num_;
    BufferQueue buffers_;
  };

  // WebMParserClient methods.
  virtual WebMParserClient* OnListStart(int id) override;
  virtual bool OnListEnd(int id) override;
  virtual bool OnUInt(int id, int64 val) override;
  virtual bool OnBinary(int id, const uint8* data, int size) override;

  bool ParseBlock(const uint8* buf, int size, int duration);
  bool OnBlock(int track_num, int timecode, int duration, int flags,
               const uint8* data, int size);

  double timecode_multiplier_;  // Multiplier used to convert timecodes into
                                // microseconds.
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;

  WebMListParser parser_;

  int64 last_block_timecode_;
  scoped_array<uint8> block_data_;
  int block_data_size_;
  int64 block_duration_;

  int64 cluster_timecode_;
  base::TimeDelta cluster_start_time_;
  bool cluster_ended_;

  Track audio_;
  Track video_;

  LogCB log_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMClusterParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_
