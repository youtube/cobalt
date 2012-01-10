// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_H_
#define MEDIA_BASE_STREAM_PARSER_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderConfig;
class Buffer;
class VideoDecoderConfig;

// Provides callback methods for StreamParser to report information
// about the media stream.
class MEDIA_EXPORT StreamParserHost {
 public:
  typedef std::deque<scoped_refptr<Buffer> > BufferQueue;

  StreamParserHost();
  virtual ~StreamParserHost();

  // A new audio decoder configuration was encountered. All audio buffers
  // after this call will be associated with this configuration.
  virtual bool OnNewAudioConfig(const AudioDecoderConfig& config) = 0;

  // A new video decoder configuration was encountered. All video buffers
  // after this call will be associated with this configuration.
  virtual bool OnNewVideoConfig(const VideoDecoderConfig& config) = 0;

  // New audio buffers have been received.
  virtual bool OnAudioBuffers(const BufferQueue& buffers) = 0;

  // New video buffers have been received.
  virtual bool OnVideoBuffers(const BufferQueue& buffers) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamParserHost);
};

// Abstract interface for parsing media byte streams.
class MEDIA_EXPORT StreamParser {
 public:
  StreamParser();
  virtual ~StreamParser();

  // Indicates completion of parser initialization.
  // First parameter - Indicates initialization success. Set to true if
  //                   initialization was successful. False if an error
  //                   occurred.
  // Second parameter -  Indicates the stream duration. Only contains a valid
  //                     value if the first parameter is true.
  typedef base::Callback<void(bool, base::TimeDelta)> InitCB;

  // Initialize the parser with necessary callbacks. Must be called before any
  // data is passed to Parse(). |init_cb| will be called once enough data has
  // been parsed to determine the initial stream configurations, presentation
  // start time, and duration.
  virtual void Init(const InitCB& init_cb, StreamParserHost* host) = 0;

  // Called when a seek occurs. This flushes the current parser state
  // and puts the parser in a state where it can receive data for the new seek
  // point.
  virtual void Flush() = 0;

  // Called when there is new data to parse.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  virtual int Parse(const uint8* buf, int size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamParser);
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_H_
