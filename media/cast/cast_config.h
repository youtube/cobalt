// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_CONFIG_H_
#define MEDIA_CAST_CAST_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace media {
class VideoEncodeAccelerator;
enum class AudioCodec;
enum class VideoCodec;

namespace cast {

// TODO(https://crbug.com/1363514): should be removed in favor of
// media::VideoCodec, media::AudioCodec.
enum class Codec {
  kUnknown,
  kAudioOpus,
  kAudioPcm16,
  kAudioAac,
  kAudioRemote,

  // For tests only.  Must set enable_fake_codec_for_tests to true.
  kVideoFake,
  kVideoVp8,
  kVideoH264,
  kVideoRemote,
  kVideoVp9,
  kVideoAv1,
  kMaxValue = kVideoAv1
};

AudioCodec ToAudioCodec(Codec codec);
VideoCodec ToVideoCodec(Codec codec);

// Describes the content being transported over RTP streams.
enum class RtpPayloadType {
  UNKNOWN = -1,

  // Cast Streaming will encode raw audio frames using one of its available
  // codec implementations, and transport encoded data in the RTP stream.
  FIRST = 96,
  AUDIO_OPUS = 96,
  AUDIO_AAC = 97,
  AUDIO_PCM16 = 98,

  // Audio frame data is not modified, and should be transported reliably and
  // in-sequence. No assumptions about the data can be made.
  REMOTE_AUDIO = 99,

  AUDIO_LAST = REMOTE_AUDIO,

  // Cast Streaming will encode raw video frames using one of its available
  // codec implementations, and transport encoded data in the RTP stream.
  VIDEO_VP8 = 100,
  VIDEO_H264 = 101,

  // Video frame data is not modified, and should be transported reliably and
  // in-sequence. No assumptions about the data can be made.
  REMOTE_VIDEO = 102,

  VIDEO_VP9 = 103,

  VIDEO_AV1 = 104,

  LAST = VIDEO_AV1
};

// Desired end-to-end latency.
constexpr base::TimeDelta kDefaultTargetPlayoutDelay = base::Milliseconds(400);

enum SuggestedDefaults {
  // Audio encoder bitrate.  Zero means "auto," which asks the encoder to select
  // a bitrate that dynamically adjusts to the content.  Otherwise, a constant
  // bitrate is used.
  kDefaultAudioEncoderBitrate = 0,

  // Suggested default audio sampling rate.
  kDefaultAudioSamplingRate = 48000,

  // RTP timebase for media remoting RTP streams.
  kRemotingRtpTimebase = 90000,

  // Suggested default maximum video frame rate.
  kDefaultMaxFrameRate = 30,

  // Suggested minimum and maximum video bitrates for general-purpose use (up to
  // 1080p, 30 FPS).
  kDefaultMinVideoBitrate = 300000,
  kDefaultMaxVideoBitrate = 5000000,

  // Minimum and Maximum VP8 quantizer in default configuration.
  kDefaultMaxQp = 63,
  kDefaultMinQp = 4,

  kDefaultMaxCpuSaverQp = 25,

  // Number of video buffers in default configuration (applies only to certain
  // external codecs).
  kDefaultNumberOfVideoBuffers = 1,
};

// These parameters are only for video encoders.
struct VideoCodecParams {
  VideoCodecParams();
  VideoCodecParams(const VideoCodecParams& other);
  VideoCodecParams(VideoCodecParams&& other);
  VideoCodecParams& operator=(const VideoCodecParams& other);
  VideoCodecParams& operator=(VideoCodecParams&& other);
  ~VideoCodecParams();

  int max_qp = kDefaultMaxQp;
  int min_qp = kDefaultMinQp;

  // The maximum |min_quantizer| set to the encoder when CPU is constrained.
  // This is a trade-off between higher resolution with lower encoding quality
  // and lower resolution with higher encoding quality. The set value indicates
  // the maximum quantizer that the encoder might produce better quality video
  // at this resolution than lowering resolution with similar CPU usage and
  // smaller quantizer. The set value has to be between |min_qp| and |max_qp|.
  // Suggested value range: [4, 30]. It is only used by software VP8 codec.
  int max_cpu_saver_qp = kDefaultMaxCpuSaverQp;

  // This field is used differently by various encoders.
  //
  // It defaults to 1.
  //
  // For VP8, this field is ignored.
  //
  // For H.264 on Mac or iOS, it controls the max number of frames the encoder
  // may hold before emitting a frame. A larger window may allow higher encoding
  // efficiency at the cost of latency and memory. Set to 0 to let the encoder
  // choose a suitable value for the platform and other encoding settings.
  int max_number_of_video_buffers_used = kDefaultNumberOfVideoBuffers;

  int number_of_encode_threads = 1;
};

struct FrameSenderConfig {
  FrameSenderConfig();
  FrameSenderConfig(uint32_t sender_ssrc,
                    uint32_t receiver_ssrc,
                    base::TimeDelta min_playout_delay,
                    base::TimeDelta max_playout_delay,
                    RtpPayloadType rtp_payload_type,
                    bool use_hardware_encoder,
                    int rtp_timebase,
                    int channels,
                    int max_bitrate,
                    int min_bitrate,
                    int start_bitrate,
                    double max_frame_rate,
                    Codec codec,
                    std::string aes_key,
                    std::string aes_iv_mask,
                    VideoCodecParams video_codec_params);
  FrameSenderConfig(const FrameSenderConfig& other);
  FrameSenderConfig(FrameSenderConfig&& other);
  FrameSenderConfig& operator=(const FrameSenderConfig& other);
  FrameSenderConfig& operator=(FrameSenderConfig&& other);
  ~FrameSenderConfig();

  // The sender's SSRC identifier.
  uint32_t sender_ssrc = 0;

  // The receiver's SSRC identifier.
  uint32_t receiver_ssrc = 0;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This should be
  // set to a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  //
  // All three delays are set to the same value due to adaptive latency
  // being disabled in Chrome.
  // TODO(https://crbug.com/1363017): re-enable adaptive playout dleay.
  base::TimeDelta min_playout_delay = kDefaultTargetPlayoutDelay;
  base::TimeDelta max_playout_delay = kDefaultTargetPlayoutDelay;

  // RTP payload type enum: Specifies the type/encoding of frame data.
  RtpPayloadType rtp_payload_type = RtpPayloadType::UNKNOWN;
  bool is_audio() const {
    return rtp_payload_type >= media::cast::RtpPayloadType::FIRST &&
           rtp_payload_type <= media::cast::RtpPayloadType::AUDIO_LAST;
  }

  // If true, use an external HW encoder rather than the built-in
  // software-based one. Note that this may be the ExternalVideoEncoder or
  // the H264VideoToolboxEncoder as appropriate.
  bool use_hardware_encoder = false;

  // RTP timebase: The number of RTP units advanced per one second.  For audio,
  // this is the sampling rate.  For video, by convention, this is 90 kHz.
  int rtp_timebase = 0;

  // Number of channels.  For audio, this is normally 2.  For video, this must
  // be 1 as Cast does not have support for stereoscopic video.
  int channels = 0;

  // For now, only fixed bitrate is used for audio encoding. So for audio,
  // |max_bitrate| is used, and the other two will be overriden if they are not
  // equal to |max_bitrate|.
  int max_bitrate = 0;
  int min_bitrate = 0;
  int start_bitrate = 0;

  double max_frame_rate = kDefaultMaxFrameRate;

  // Codec used for the compression of signal data.
  Codec codec = Codec::kUnknown;

  // The AES crypto key and initialization vector.  Each of these strings
  // contains the data in binary form, of size kAesKeySize.  If they are empty
  // strings, crypto is not being used.
  std::string aes_key;
  std::string aes_iv_mask;

  // When true, allows use of Codec::kVideoFake.  When false, Codec::kVideoFake
  // is not supported.
  bool enable_fake_codec_for_tests = false;

  // These are codec specific parameters for video streams only.
  VideoCodecParams video_codec_params;
};

struct FrameReceiverConfig {
  FrameReceiverConfig();
  FrameReceiverConfig(const FrameReceiverConfig& other);
  FrameReceiverConfig(FrameReceiverConfig&& other);
  FrameReceiverConfig& operator=(const FrameReceiverConfig& other);
  FrameReceiverConfig& operator=(FrameReceiverConfig&& other);
  ~FrameReceiverConfig();

  // The receiver's SSRC identifier.
  uint32_t receiver_ssrc = 0;

  // The sender's SSRC identifier.
  uint32_t sender_ssrc = 0;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  int rtp_max_delay_ms = kDefaultTargetPlayoutDelay.InMilliseconds();

  // RTP payload type enum: Specifies the type/encoding of frame data.
  RtpPayloadType rtp_payload_type = RtpPayloadType::UNKNOWN;

  // RTP timebase: The number of RTP units advanced per one second.  For audio,
  // this is the sampling rate.  For video, by convention, this is 90 kHz.
  int rtp_timebase = 0;

  // Number of channels.  For audio, this is normally 2.  For video, this must
  // be 1 as Cast does not have support for stereoscopic video.
  int channels = 0;

  // The target frame rate.  For audio, this is normally 100 (i.e., frames have
  // a duration of 10ms each).  For video, this is normally 30, but any frame
  // rate is supported.
  double target_frame_rate = 0;

  // Codec used for the compression of signal data.
  Codec codec = Codec::kUnknown;

  // The AES crypto key and initialization vector.  Each of these strings
  // contains the data in binary form, of size kAesKeySize.  If they are empty
  // strings, crypto is not being used.
  std::string aes_key;
  std::string aes_iv_mask;
};

typedef base::OnceCallback<void(scoped_refptr<base::SingleThreadTaskRunner>,
                                std::unique_ptr<media::VideoEncodeAccelerator>)>
    ReceiveVideoEncodeAcceleratorCallback;
typedef base::RepeatingCallback<void(ReceiveVideoEncodeAcceleratorCallback)>
    CreateVideoEncodeAcceleratorCallback;
typedef base::OnceCallback<void(base::UnsafeSharedMemoryRegion)>
    ReceiveVideoEncodeMemoryCallback;
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_CONFIG_H_
