// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame_pool.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace media {

class DecoderBuffer;
class FFmpegDecodingLoop;
class MediaLog;

class MEDIA_EXPORT FFmpegVideoDecoder : public VideoDecoder {
 public:
  static bool IsCodecSupported(VideoCodec codec);
  static SupportedVideoDecoderConfigs SupportedConfigsForWebRTC();

  explicit FFmpegVideoDecoder(MediaLog* media_log);

  FFmpegVideoDecoder(const FFmpegVideoDecoder&) = delete;
  FFmpegVideoDecoder& operator=(const FFmpegVideoDecoder&) = delete;

  ~FFmpegVideoDecoder() override;

  // Allow decoding of individual NALU. Entire frames are required by default.
  // Disables low-latency mode. Must be called before Initialize().
  void set_decode_nalus(bool decode_nalus) { decode_nalus_ = decode_nalus; }

  // VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

  // Callback called from within FFmpeg to allocate a buffer based on
  // the dimensions of |codec_context|. See AVCodecContext.get_buffer2
  // documentation inside FFmpeg.
  int GetVideoBuffer(struct AVCodecContext* codec_context,
                     AVFrame* frame,
                     int flags);

  void force_allocation_error_for_testing() { force_allocation_error_ = true; }

 private:
  enum class DecoderState { kUninitialized, kNormal, kDecodeFinished, kError };

  // Handles decoding of an unencrypted encoded buffer. A return value of false
  // indicates that an error has occurred.
  bool FFmpegDecode(const DecoderBuffer& buffer);
  bool OnNewFrame(AVFrame* frame);

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true if initialization was successful.
  bool ConfigureDecoder(const VideoDecoderConfig& config, bool low_delay);

  // Releases resources associated with |codec_context_|.
  void ReleaseFFmpegResources();

  SEQUENCE_CHECKER(sequence_checker_);

  MediaLog* const media_log_;

  DecoderState state_ = DecoderState::kUninitialized;

  OutputCB output_cb_;

  // FFmpeg structures owned by this object.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  VideoDecoderConfig config_;

  VideoFramePool frame_pool_;

  bool decode_nalus_ = false;

  bool force_allocation_error_ = false;

  std::unique_ptr<FFmpegDecodingLoop> decoding_loop_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
