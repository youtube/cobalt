// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_DEMUXER_STREAM_H_
#define COBALT_MEDIA_BASE_DEMUXER_STREAM_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/video_rotation.h"

namespace media {

class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;

class MEDIA_EXPORT DemuxerStream {
 public:
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO,
    TEXT,
    NUM_TYPES,  // Always keep this entry as the last one!
  };

  enum Liveness {
    LIVENESS_UNKNOWN,
    LIVENESS_RECORDED,
    LIVENESS_LIVE,
  };

  // Status returned in the Read() callback.
  //  kOk : Indicates the second parameter is Non-NULL and contains media data
  //        or the end of the stream.
  //  kAborted : Indicates an aborted Read(). This can happen if the
  //             DemuxerStream gets flushed and doesn't have any more data to
  //             return. The second parameter MUST be NULL when this status is
  //             returned.
  //  kConfigChange : Indicates that the AudioDecoderConfig or
  //                  VideoDecoderConfig for the stream has changed.
  //                  The DemuxerStream expects an audio_decoder_config() or
  //                  video_decoder_config() call before Read() will start
  //                  returning DecoderBuffers again. The decoder will need this
  //                  new configuration to properly decode the buffers read
  //                  from this point forward. The second parameter MUST be NULL
  //                  when this status is returned.
  //                  This will only be returned if SupportsConfigChanges()
  //                  returns 'true' for this DemuxerStream.
  enum Status {
    kOk,
    kAborted,
    kConfigChanged,
  };

  // Request a buffer to returned via the provided callback.
  //
  // The first parameter indicates the status of the read.
  // The second parameter is non-NULL and contains media data
  // or the end of the stream if the first parameter is kOk. NULL otherwise.
  typedef base::Callback<void(Status, const scoped_refptr<DecoderBuffer>&)>
      ReadCB;
  virtual void Read(const ReadCB& read_cb) = 0;

  // Returns the audio/video decoder configuration. It is an error to call the
  // audio method on a video stream and vice versa. After |kConfigChanged| is
  // returned in a Read(), the caller should call this method again to retrieve
  // the new config.
  virtual AudioDecoderConfig audio_decoder_config() = 0;
  virtual VideoDecoderConfig video_decoder_config() = 0;

  // Returns the type of stream.
  virtual Type type() const = 0;

  // Returns liveness of the streams provided, i.e. whether recorded or live.
  virtual Liveness liveness() const;

  virtual void EnableBitstreamConverter();

  // Whether or not this DemuxerStream allows midstream configuration changes.
  //
  // A DemuxerStream that returns 'true' to this may return the 'kConfigChange'
  // status from a Read() call. In this case the client is expected to be
  // capable of taking appropriate action to handle config changes. Otherwise
  // audio_decoder_config() and video_decoder_config()'s return values are
  // guaranteed to remain constant, and the client may make optimizations based
  // on this.
  virtual bool SupportsConfigChanges() = 0;

  virtual VideoRotation video_rotation() = 0;

  // Indicates whether a DemuxerStream is currently enabled (i.e. should be
  // decoded and rendered) or not.
  virtual bool enabled() const = 0;

  // Disables and re-enables the stream. Reading from a disabled stream will
  // return an end-of-stream (EOS) buffer. When a stream is re-enabled, it needs
  // to know the current playback position |timestamp| in order to resume
  // reading data from a key frame preceeding the |timestamp|.
  virtual void set_enabled(bool enabled, base::TimeDelta timestamp) = 0;

  // The StreamStatusChangeCB allows DemuxerStream clients to receive
  // notifications about the stream being disabled or enabled.
  // The first parameter indicates whether the stream is enabled or disabled.
  // The second parameter is the playback position when the change occured.
  typedef base::Callback<void(bool, base::TimeDelta)> StreamStatusChangeCB;
  virtual void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) = 0;

 protected:
  // Only allow concrete implementations to get deleted.
  virtual ~DemuxerStream();
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_DEMUXER_STREAM_H_
