// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_STREAM_PARSER_H_
#define COBALT_MEDIA_BASE_STREAM_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/eme_constants.h"
#include "cobalt/media/base/media_export.h"

namespace media {

class MediaLog;
class MediaTracks;
class StreamParserBuffer;
class TextTrackConfig;

// Abstract interface for parsing media byte streams.
class MEDIA_EXPORT StreamParser {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  // Range of |TrackId| is dependent upon stream parsers. It is currently
  // the key for the buffer's text track config in the applicable
  // TextTrackConfigMap (which is passed in StreamParser::NewConfigCB), or
  // 0 for other media types that currently allow at most one track.
  // WebMTracksParser uses -1 as an invalid text track number.
  // It is also the key for BufferQueueMap structure returned by stream parsers.
  // TODO(servolk/wolenetz): Change to size_type or unsigned after fixing track
  // id handling in FrameProcessor.
  typedef int TrackId;

  // Map of text track ID to the track configuration.
  typedef std::map<TrackId, TextTrackConfig> TextTrackConfigMap;

  // Map of track ID to decode-timestamp-ordered buffers for the track.
  typedef std::map<TrackId, BufferQueue> BufferQueueMap;

  // Stream parameters passed in InitCB.
  struct MEDIA_EXPORT InitParameters {
    explicit InitParameters(base::TimeDelta duration);

    // Stream duration.
    base::TimeDelta duration;

    // Indicates the source time associated with presentation timestamp 0. A
    // null Time is returned if no mapping to Time exists.
    base::Time timeline_offset;

    // Indicates that timestampOffset should be updated based on the earliest
    // end timestamp (audio or video) provided during each NewBuffersCB.
    bool auto_update_timestamp_offset;

    // Indicates live stream.
    DemuxerStream::Liveness liveness;

    // Counts of tracks detected by type within this stream. Not all of these
    // tracks may be selected for use by the parser.
    int detected_audio_track_count;
    int detected_video_track_count;
    int detected_text_track_count;
  };

  // Indicates completion of parser initialization.
  //   params - Stream parameters.
  typedef base::Callback<void(const InitParameters& params)> InitCB;

  // Indicates when new stream configurations have been parsed.
  // First parameter - An object containing information about media tracks as
  //                   well as audio/video decoder configs associated with each
  //                   track the parser will use from the stream.
  // Second parameter - The new text tracks configuration.  If the map is empty,
  //                    then no text tracks were parsed for use from the stream.
  // Return value - True if the new configurations are accepted.
  //                False if the new configurations are not supported
  //                and indicates that a parsing error should be signalled.
  typedef base::Callback<bool(scoped_ptr<MediaTracks>,
                              const TextTrackConfigMap&)> NewConfigCB;

  // New stream buffers have been parsed.
  // First parameter - A map of track ids to queues of newly parsed buffers.
  // Return value - True indicates that the buffers are accepted.
  //                False if something was wrong with the buffers and a parsing
  //                error should be signalled.
  typedef base::Callback<bool(const BufferQueueMap&)> NewBuffersCB;

  // Signals the beginning of a new media segment.
  typedef base::Callback<void()> NewMediaSegmentCB;

  // Signals the end of a media segment.
  typedef base::Callback<void()> EndMediaSegmentCB;

  // A new potentially encrypted stream has been parsed.
  // First parameter - The type of the initialization data associated with the
  //                   stream.
  // Second parameter - The initialization data associated with the stream.
  typedef base::Callback<void(EmeInitDataType, const std::vector<uint8_t>&)>
      EncryptedMediaInitDataCB;

  StreamParser();
  virtual ~StreamParser();

  // Initializes the parser with necessary callbacks. Must be called before any
  // data is passed to Parse(). |init_cb| will be called once enough data has
  // been parsed to determine the initial stream configurations, presentation
  // start time, and duration. If |ignore_text_track| is true, then no text
  // buffers should be passed later by the parser to |new_buffers_cb|.
  virtual void Init(
      const InitCB& init_cb, const NewConfigCB& config_cb,
      const NewBuffersCB& new_buffers_cb, bool ignore_text_track,
      const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
      const NewMediaSegmentCB& new_segment_cb,
      const EndMediaSegmentCB& end_of_segment_cb,
      const scoped_refptr<MediaLog>& media_log) = 0;

  // Called during the reset parser state algorithm.  This flushes the current
  // parser and puts the parser in a state where it can receive data.  This
  // method does not need to invoke the EndMediaSegmentCB since the parser reset
  // algorithm already resets the segment parsing state.
  virtual void Flush() = 0;

  // Called when there is new data to parse.
  //
  // Returns true if the parse succeeds.
  virtual bool Parse(const uint8_t* buf, int size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamParser);
};

// Appends to |merged_buffers| the provided buffers in decode-timestamp order.
// Any previous contents of |merged_buffers| is assumed to have lower
// decode timestamps versus the provided buffers. All provided buffer queues
// are assumed to already be in decode-timestamp order.
// Returns false if any of the provided audio/video/text buffers are found
// to not be in decode timestamp order, or have a decode timestamp less than
// the last buffer, if any, in |merged_buffers|. Partial results may exist
// in |merged_buffers| in this case. Returns true on success.
// No validation of media type within the various buffer queues is done here.
// TODO(wolenetz/acolwell): Merge incrementally in parsers to eliminate
// subtle issues with tie-breaking. See http://crbug.com/338484.
MEDIA_EXPORT bool MergeBufferQueues(const StreamParser::BufferQueueMap& buffers,
                                    StreamParser::BufferQueue* merged_buffers);

}  // namespace media

#endif  // COBALT_MEDIA_BASE_STREAM_PARSER_H_
