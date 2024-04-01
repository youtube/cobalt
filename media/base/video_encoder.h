// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_ENCODER_H_
#define MEDIA_BASE_VIDEO_ENCODER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

// Encoded video frame, its data and metadata.
struct MEDIA_EXPORT VideoEncoderOutput {
  VideoEncoderOutput();
  VideoEncoderOutput(VideoEncoderOutput&&);
  ~VideoEncoderOutput();

  // Feel free take this buffer out and use underlying memory as is without
  // copying.
  std::unique_ptr<uint8_t[]> data;
  size_t size = 0;

  base::TimeDelta timestamp;
  bool key_frame = false;
  int temporal_id = 0;
};

class MEDIA_EXPORT VideoEncoder {
 public:
  // TODO: Move this to a new file if there are more codec specific options.
  struct MEDIA_EXPORT AvcOptions {
    bool produce_annexb = false;
  };

  enum class LatencyMode { Realtime, Quality };

  struct MEDIA_EXPORT Options {
    Options();
    Options(const Options&);
    ~Options();
    absl::optional<Bitrate> bitrate;
    absl::optional<double> framerate;

    gfx::Size frame_size;

    absl::optional<int> keyframe_interval = 10000;

    LatencyMode latency_mode = LatencyMode::Realtime;

    absl::optional<SVCScalabilityMode> scalability_mode;

    // Only used for H264 encoding.
    AvcOptions avc;
  };

  // A sequence of codec specific bytes, commonly known as extradata.
  // If available, it should be given to the decoder as part of the
  // decoder config.
  using CodecDescription = std::vector<uint8_t>;

  // Callback for VideoEncoder to report an encoded video frame whenever it
  // becomes available.
  using OutputCB =
      base::RepeatingCallback<void(VideoEncoderOutput output,
                                   absl::optional<CodecDescription>)>;

  // Callback to report success and errors in encoder calls.
  using StatusCB = base::OnceCallback<void(Status error)>;

  struct PendingEncode {
    PendingEncode();
    PendingEncode(PendingEncode&&);
    ~PendingEncode();
    StatusCB done_callback;
    scoped_refptr<VideoFrame> frame;
    bool key_frame;
  };

  VideoEncoder();
  VideoEncoder(const VideoEncoder&) = delete;
  VideoEncoder& operator=(const VideoEncoder&) = delete;
  virtual ~VideoEncoder();

  // Initializes a VideoEncoder with the given |options|, executing the
  // |done_cb| upon completion. |output_cb| is called for each encoded frame
  // produced by the coder.
  //
  // Note:
  // 1) Can't be called more than once for the same instance of the encoder.
  // 2) No VideoEncoder calls should be made before |done_cb| is executed.
  virtual void Initialize(VideoCodecProfile profile,
                          const Options& options,
                          OutputCB output_cb,
                          StatusCB done_cb) = 0;

  // Requests a |frame| to be encoded. The status of the encoder and the frame
  // are returned via the provided callback |done_cb|.
  //
  // |done_cb| will not be called from within this method, and that it will be
  // called even if Encode() is never called again.

  // After the frame, or several frames, are encoded the encoder calls
  // |output_cb| specified in Initialize() for available VideoEncoderOutput.
  // |output_cb| may be called before or after |done_cb|,
  // including before Encode() returns.
  // Encode() does not expect EOS frames, use Flush() to finalize the stream
  // and harvest the outputs.
  virtual void Encode(scoped_refptr<VideoFrame> frame,
                      bool key_frame,
                      StatusCB done_cb) = 0;

  // Adjust encoder options and the output callback for future frames, executing
  // the |done_cb| upon completion.
  //
  // Note:
  // 1. Not all options can be changed on the fly.
  // 2. ChangeOptions() should be called after calling Flush() and waiting
  // for it to finish.
  virtual void ChangeOptions(const Options& options,
                             OutputCB output_cb,
                             StatusCB done_cb) = 0;

  // Requests all outputs for already encoded frames to be
  // produced via |output_cb| and calls |dene_cb| after that.
  virtual void Flush(StatusCB done_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_ENCODER_H_
