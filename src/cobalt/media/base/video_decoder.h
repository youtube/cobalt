// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_VIDEO_DECODER_H_
#define COBALT_MEDIA_BASE_VIDEO_DECODER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/media/base/decode_status.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/pipeline_status.h"
#include "ui/gfx/size.h"

namespace media {

class CdmContext;
class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

class MEDIA_EXPORT VideoDecoder {
 public:
  // Callback for VideoDecoder initialization.
  typedef base::Callback<void(bool success)> InitCB;

  // Callback for VideoDecoder to return a decoded frame whenever it becomes
  // available. Only non-EOS frames should be returned via this callback.
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> OutputCB;

  // Callback type for Decode(). Called after the decoder has completed decoding
  // corresponding DecoderBuffer, indicating that it's ready to accept another
  // buffer to decode.
  typedef base::Callback<void(DecodeStatus)> DecodeCB;

  VideoDecoder();

  // Fires any pending callbacks, stops and destroys the decoder.
  // Note: Since this is a destructor, |this| will be destroyed after this call.
  // Make sure the callbacks fired from this call doesn't post any task that
  // depends on |this|.
  virtual ~VideoDecoder();

  // Returns the name of the decoder for logging purpose.
  virtual std::string GetDisplayName() const = 0;

  // Initializes a VideoDecoder with the given |config|, executing the
  // |init_cb| upon completion. |output_cb| is called for each output frame
  // decoded by Decode().
  //
  // If |low_delay| is true then the decoder is not allowed to queue frames,
  // except for out-of-order frames, i.e. if the next frame can be returned it
  // must be returned without waiting for Decode() to be called again.
  // Initialization should fail if |low_delay| is true and the decoder cannot
  // satisfy the requirements above.
  //
  // |cdm_context| can be used to handle encrypted buffers. May be null if the
  // stream is not encrypted.
  //
  // Note:
  // 1) The VideoDecoder will be reinitialized if it was initialized before.
  //    Upon reinitialization, all internal buffered frames will be dropped.
  // 2) This method should not be called during pending decode or reset.
  // 3) No VideoDecoder calls should be made before |init_cb| is executed.
  // 4) VideoDecoders should take care to run |output_cb| as soon as the frame
  // is ready (i.e. w/o thread trampolining) since it can strongly affect frame
  // delivery times with high-frame-rate material.  See Decode() for additional
  // notes.
  virtual void Initialize(const VideoDecoderConfig& config, bool low_delay,
                          CdmContext* cdm_context, const InitCB& init_cb,
                          const OutputCB& output_cb) = 0;

  // Requests a |buffer| to be decoded. The status of the decoder and decoded
  // frame are returned via the provided callback. Some decoders may allow
  // decoding multiple buffers in parallel. Callers should call
  // GetMaxDecodeRequests() to get number of buffers that may be decoded in
  // parallel. Decoder must call |decode_cb| in the same order in which Decode()
  // is called.
  //
  // Implementations guarantee that the |decode_cb| will not be called from
  // within this method, and that it will be called even if Decode() is never
  // called again.
  //
  // After decoding is finished the decoder calls |output_cb| specified in
  // Initialize() for each decoded frame. |output_cb| may be called before or
  // after |decode_cb|, including before Decode() returns.
  //
  // If |buffer| is an EOS buffer then the decoder must be flushed, i.e.
  // |output_cb| must be called for each frame pending in the queue and
  // |decode_cb| must be called after that. Callers will not call Decode()
  // again until after the flush completes.
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  // Note: No VideoDecoder calls should be made before |closure| is executed.
  virtual void Reset(const base::Closure& closure) = 0;

  // Returns true if the decoder needs bitstream conversion before decoding.
  virtual bool NeedsBitstreamConversion() const;

  // Returns true if the decoder currently has the ability to decode and return
  // a VideoFrame. Most implementations can allocate a new VideoFrame and hence
  // this will always return true. Override and return false for decoders that
  // use a fixed set of VideoFrames for decoding.
  virtual bool CanReadWithoutStalling() const;

  // Returns maximum number of parallel decode requests.
  virtual int GetMaxDecodeRequests() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_VIDEO_DECODER_H_
