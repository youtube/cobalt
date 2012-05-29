// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_

#include <deque>
#include <list>
#include <utility>

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"

namespace media {

class SourceBufferRange;

// SourceBufferStream is a data structure that stores media Buffers in ranges.
// Buffers can be appended out of presentation order. Buffers are retrieved by
// seeking to the desired start point and calling GetNextBuffer(). Buffers are
// returned in sequential presentation order.
class MEDIA_EXPORT SourceBufferStream {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;
  typedef std::pair<base::TimeDelta, base::TimeDelta> Timespan;
  typedef std::list<Timespan> TimespanList;

  SourceBufferStream();
  explicit SourceBufferStream(const AudioDecoderConfig& audio_config);
  explicit SourceBufferStream(const VideoDecoderConfig& video_config);

  ~SourceBufferStream();

  // Add the |buffers| to the SourceBufferStream. Buffers within the queue are
  // expected to be in order, but multiple calls to Append() may add buffers out
  // of order or overlapping. Assumes all buffers within |buffers| are in
  // presentation order and are non-overlapping.
  // Returns true if Append() was successful, false if |buffers| are not added.
  // TODO(vrk): Implement proper end-overlapping. (crbug.com/125072)
  // This may trigger garbage collection.
  // TODO(vrk): Implement garbage collection. (crbug.com/125070)
  bool Append(const BufferQueue& buffers);

  // Changes the SourceBufferStream's state so that it will start returning
  // buffers starting from the closest keyframe before |timestamp|.
  void Seek(base::TimeDelta timestamp);

  // Returns true if the SourceBufferStream has seeked to a time without
  // buffered data and is waiting for more data to be appended.
  bool IsSeekPending() const;

  // Fills |out_buffer| with a new buffer. Seek() must be called before calling
  // this method. Buffers are presented in order from the last call to Seek().
  // |out_buffer|'s timestamp may be earlier than the |timestamp| passed to
  // the last Seek() call.
  // Returns true if |out_buffer| is filled with a valid buffer, false if
  // there is not enough data buffered to fulfill the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);

  // Returns a list of the buffered time ranges.
  TimespanList GetBufferedTime() const;

  // Notifies this SourceBufferStream that EndOfStream has been called and that
  // GetNextBuffer() should return EOS buffers after all other buffered data.
  // Returns false if called when there is a gap between the current position
  // and the end of the buffered data.
  void EndOfStream();

  // Returns true if this SourceBufferStream can successfully call EndOfStream()
  // (if there are no gaps between the current position and the remaining data).
  bool CanEndOfStream() const;

  const AudioDecoderConfig& GetCurrentAudioDecoderConfig() {
    return audio_config_;
  }
  const VideoDecoderConfig& GetCurrentVideoDecoderConfig() {
    return video_config_;
  }

 private:
  typedef std::list<SourceBufferRange*> RangeList;

  // Resolve overlapping ranges such that no ranges overlap anymore.
  // |range_itr| points to the iterator in |ranges_| immediately after
  // |new_range|. Returns the iterator in |ranges_| immediately after
  // |new_range|, which may be different from the original |range_itr|.
  RangeList::iterator ResolveCompleteOverlaps(
      const RangeList::iterator& range_itr, SourceBufferRange* new_range);
  RangeList::iterator ResolveEndOverlaps(
      const RangeList::iterator& range_itr, SourceBufferRange* new_range);

  // Checks to see if the range pointed to by |range_itr| can be appended to the
  // end of |new_range|, and if so, appends the range and updates |ranges_| to
  // reflect this.
  void MergeWithAdjacentRangeIfNecessary(
      const RangeList::iterator& range_itr, SourceBufferRange* new_range);

  // List of disjoint buffered ranges, ordered by start time.
  RangeList ranges_;

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  // True if more data needs to be appended before the Seek() can complete,
  // false if no Seek() has been requested or the Seek() is completed.
  bool seek_pending_;

  // Timestamp of the last request to Seek().
  base::TimeDelta seek_buffer_timestamp_;

  // Pointer to the seeked-to Range. This is the range from which
  // GetNextBuffer() calls are fulfilled after the |track_buffer_| has been
  // emptied.
  SourceBufferRange* selected_range_;

  // Queue of the next buffers to be returned from calls to GetNextBuffer(). If
  // |track_buffer_| is empty, return buffers from |selected_range_|.
  BufferQueue track_buffer_;

  // True if the next buffer after the end of the |track_buffer_| is not
  // buffered yet and we need to wait for the next keyframe after
  // |track_buffer_| to be appended.
  bool waiting_for_keyframe_;

  // True when EndOfStream() has been called and GetNextBuffer() should return
  // EOS buffers for read requests beyond the buffered data.  False initially.
  bool end_of_stream_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
