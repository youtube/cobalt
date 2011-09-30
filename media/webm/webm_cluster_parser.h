// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_
#define MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_

#include <deque>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/buffers.h"
#include "media/webm/webm_parser.h"

namespace media {

class WebMClusterParser : public WebMParserClient {
 public:
  typedef std::deque<scoped_refptr<Buffer> > BufferQueue;

  WebMClusterParser(int64 timecode_scale,
                    int audio_track_num,
                    base::TimeDelta audio_default_duration,
                    int video_track_num,
                    base::TimeDelta video_default_duration);
  virtual ~WebMClusterParser();

  // Parses a WebM cluster element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  const BufferQueue& audio_buffers() const { return audio_buffers_; }
  const BufferQueue& video_buffers() const { return video_buffers_; }

 private:
  // WebMParserClient methods.
  virtual bool OnListStart(int id);
  virtual bool OnListEnd(int id);
  virtual bool OnUInt(int id, int64 val);
  virtual bool OnFloat(int id, double val);
  virtual bool OnBinary(int id, const uint8* data, int size);
  virtual bool OnString(int id, const std::string& str);
  virtual bool OnSimpleBlock(int track_num, int timecode, int flags,
                             const uint8* data, int size);

  double timecode_multiplier_;  // Multiplier used to convert timecodes into
                                // microseconds.
  int audio_track_num_;
  base::TimeDelta audio_default_duration_;
  int video_track_num_;
  base::TimeDelta  video_default_duration_;

  int64 last_block_timecode_;

  int64 cluster_timecode_;
  BufferQueue audio_buffers_;
  BufferQueue video_buffers_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMClusterParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_
