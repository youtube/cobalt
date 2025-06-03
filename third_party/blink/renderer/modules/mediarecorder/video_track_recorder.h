// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VIDEO_TRACK_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VIDEO_TRACK_RECORDER_H_

#include <atomic>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_pool.h"
#include "media/muxers/webm_muxer.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/web/modules/mediastream/encoded_video_frame.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediarecorder/buildflags.h"
#include "third_party/blink/renderer/modules/mediarecorder/key_frame_request_processor.h"
#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {
class VideoEncoderMetricsProvider;
class VideoFrame;
}

namespace video_track_recorder {
#if BUILDFLAG(IS_ANDROID)
const int kVEAEncoderMinResolutionWidth = 176;
const int kVEAEncoderMinResolutionHeight = 144;
#else
const int kVEAEncoderMinResolutionWidth = 640;
const int kVEAEncoderMinResolutionHeight = 480;
#endif
}  // namespace video_track_recorder

namespace blink {

class MediaStreamVideoTrack;

// Base class serving as interface for eventually saving encoded frames stemming
// from media from a source.
class VideoTrackRecorder : public TrackRecorder<MediaStreamVideoSink> {
 public:
  // Do not change the order of codecs; add new ones right before kLast.
  enum class CodecId {
    kVp8,
    kVp9,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    kH264,
#endif
    kAv1,
    kLast
  };

  // Callback interface for VideoTrackRecorders. The methods here need to all be
  // called on the main thread.
  class CallbackInterface : public GarbageCollectedMixin {
   public:
    // Called to indicate there is encoded passthrough (incoming encoded video
    // which wasn't encoded by MediaRecorder) video data available.
    // |encoded_alpha| represents the encode output of alpha channel when
    // available, can be empty otherwise.
    virtual void OnPassthroughVideo(const media::Muxer::VideoParameters& params,
                                    std::string encoded_data,
                                    std::string encoded_alpha,
                                    base::TimeTicks timestamp,
                                    bool is_key_frame) = 0;

    // Called to indicate there is encoded video data available. |encoded_alpha|
    // represents the encode output of alpha channel when available, can be
    // empty otherwise.
    virtual void OnEncodedVideo(
        const media::Muxer::VideoParameters& params,
        std::string encoded_data,
        std::string encoded_alpha,
        absl::optional<media::VideoEncoder::CodecDescription> codec_description,
        base::TimeTicks timestamp,
        bool is_key_frame) = 0;

    virtual std::unique_ptr<media::VideoEncoderMetricsProvider>
    CreateVideoEncoderMetricsProvider() = 0;

    // Called on encountering encoder errors.
    virtual void OnVideoEncodingError() = 0;

    // Called when a track's ready state changes.
    virtual void OnSourceReadyStateChanged() = 0;
  };

  // Video codec and its encoding profile/level.
  struct MODULES_EXPORT CodecProfile {
    CodecId codec_id;
    absl::optional<media::VideoCodecProfile> profile;
    absl::optional<uint8_t> level;

    explicit CodecProfile(CodecId codec_id);
    CodecProfile(CodecId codec_id,
                 absl::optional<media::VideoCodecProfile> opt_profile,
                 absl::optional<uint8_t> opt_level);
    CodecProfile(CodecId codec_id,
                 media::VideoCodecProfile profile,
                 uint8_t level);
  };

  using OnEncodedVideoCB = base::RepeatingCallback<void(
      const media::Muxer::VideoParameters& params,
      std::string encoded_data,
      std::string encoded_alpha,
      absl::optional<media::VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks capture_timestamp,
      bool is_key_frame)>;
  using OnErrorCB = base::RepeatingClosure;

  // MediaStreamVideoSink implementation
  double GetRequiredMinFramesPerSec() const override { return 1; }

  // Wraps a counter in a class in order to enable use of base::WeakPtr<>.
  // See https://crbug.com/859610 for why this was added.
  class Counter {
   public:
    Counter();
    ~Counter();
    uint32_t count() const { return count_; }
    void IncreaseCount();
    void DecreaseCount();
    base::WeakPtr<Counter> GetWeakPtr();

   private:
    uint32_t count_;
    base::WeakPtrFactory<Counter> weak_factory_{this};
  };

  // Base class to describe a generic Encoder, encapsulating all actual encoder
  // (re)configurations, encoding and delivery of received frames. The class is
  // fully operated on a codec-specific SequencedTaskRunner.
  class MODULES_EXPORT Encoder {
   public:
    Encoder(scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
            const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
            uint32_t bits_per_second);
    virtual ~Encoder();

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    // Should be called shortly after wrapping the Encoder in a SequenceBound,
    // on the codec-specific task runner.
    void InitializeEncoder(
        KeyFrameRequestProcessor::Configuration key_frame_config,
        std::unique_ptr<media::VideoEncoderMetricsProvider> metrics_provider);

    // Start encoding |frame|, returning via |on_encoded_video_cb_|. This
    // call will also trigger an encode configuration upon first frame arrival
    // or parameter change, and an Encode() to actually
    // encode the frame. If the |frame|'s data is not directly available (e.g.
    // it's a texture) then MaybeProvideEncodableFrame() is called, and if
    // even that fails, black frames are sent instead.
    void StartFrameEncode(
        WTF::CrossThreadRepeatingFunction<base::TimeTicks()> time_now_callback,
        scoped_refptr<media::VideoFrame> video_frame,
        base::TimeTicks capture_timestamp);

    // Like StartFrameEncode but using base::TimeTicks::Now in
    // `time_now_callback`.
    void StartFrameEncodeWithTimeTicksNow(
        scoped_refptr<media::VideoFrame> video_frame,
        base::TimeTicks capture_timestamp);

    using OnEncodedVideoInternalCB = WTF::CrossThreadFunction<void(
        const media::Muxer::VideoParameters& params,
        std::string encoded_data,
        std::string encoded_alpha,
        base::TimeTicks capture_timestamp,
        bool is_key_frame)>;

    void SetPaused(bool paused);
    virtual bool CanEncodeAlphaChannel() const;
    virtual bool IsScreenContentEncodingForTesting() const { return false; }
    virtual base::WeakPtr<Encoder> GetWeakPtr() = 0;
    void ForceKeyFrameForNextFrameForTesting() {
      request_key_frame_for_testing_ = true;
    }
   protected:
    friend class VideoTrackRecorderTest;

    scoped_refptr<media::VideoFrame> MaybeProvideEncodableFrame(
        scoped_refptr<media::VideoFrame> video_frame);
    // Called shortly after wrapping the Encoder in a SequenceBound, on the
    // codec-specific task runner.
    virtual void Initialize();
    virtual void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                             base::TimeTicks capture_timestamp,
                             bool request_keyframe) = 0;

    // Called when the frame reference is released after encode.
    void FrameReleased(scoped_refptr<media::VideoFrame> frame);

    // A helper function to convert the given |frame| to an I420 video frame.
    // Used mainly by the software encoders since I420 is the only supported
    // pixel format.  The function is best-effort.  If for any reason the
    // conversion fails, the original |frame| will be returned.
    scoped_refptr<media::VideoFrame> ConvertToI420ForSoftwareEncoder(
        scoped_refptr<media::VideoFrame> frame);

    const scoped_refptr<base::SequencedTaskRunner> encoding_task_runner_;

    // While |paused_|, frames are not encoded.
    bool paused_ = false;

    // Callback transferring encoded video data.
    const OnEncodedVideoCB on_encoded_video_cb_;

    // Target bitrate for video encoding. If 0, a standard bitrate is used.
    const uint32_t bits_per_second_;

    // Testing state - decides if we should request a key frame next round.
    bool request_key_frame_for_testing_ = false;

    // Number of frames that we keep the reference alive for encode.
    std::unique_ptr<Counter> num_frames_in_encode_;

    // Used to retrieve incoming opaque VideoFrames (i.e. VideoFrames backed by
    // textures).
    std::unique_ptr<media::PaintCanvasVideoRenderer> video_renderer_;
    SkBitmap bitmap_;
    std::unique_ptr<cc::PaintCanvas> canvas_;
    std::unique_ptr<WebGraphicsContext3DProvider> encoder_thread_context_;
    KeyFrameRequestProcessor key_frame_processor_;
    bool awaiting_first_frame_ = true;
    std::vector<uint8_t> resize_buffer_
        ALLOW_DISCOURAGED_TYPE("Avoids conversion when passed to media:: code");
    std::unique_ptr<media::VideoEncoderMetricsProvider> metrics_provider_;

    media::VideoFramePool frame_pool_;
  };

  // Class to encapsulate the enumeration of CodecIds/VideoCodecProfiles
  // supported by the VEA underlying platform. Provides methods to query the
  // preferred CodecId and to check if a given CodecId is supported.
  class MODULES_EXPORT CodecEnumerator {
   public:
    explicit CodecEnumerator(
        const media::VideoEncodeAccelerator::SupportedProfiles&
            vea_supported_profiles);

    CodecEnumerator(const CodecEnumerator&) = delete;
    CodecEnumerator& operator=(const CodecEnumerator&) = delete;

    ~CodecEnumerator();

    // Returns the first CodecId that has an associated VEA VideoCodecProfile,
    // or VP8 if none available.
    CodecId GetPreferredCodecId(MediaTrackContainerType type) const;

    // Returns supported VEA VideoCodecProfile which matches |codec| and
    // |profile| and whether VEA supports VBR encoding for the profile.
    std::pair<media::VideoCodecProfile, bool> FindSupportedVideoCodecProfile(
        CodecId codec,
        media::VideoCodecProfile profile) const;

    // Returns VEA's first supported VideoCodedProfile for a given CodecId and
    // whether VBR encoding is supported by VEA for the profile, or
    // VIDEO_CODEC_PROFILE_UNKNOWN otherwise.
    std::pair<media::VideoCodecProfile, bool>
    GetFirstSupportedVideoCodecProfile(CodecId codec) const;

    // Returns a list of supported media::VEA::SupportedProfile for a given
    // CodecId, or empty vector if CodecId is unsupported.
    media::VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
        CodecId codec) const;

   private:
    // VEA-supported profiles grouped by CodecId.
    HashMap<CodecId, media::VideoEncodeAccelerator::SupportedProfiles>
        supported_profiles_;
    CodecId preferred_codec_id_ = CodecId::kLast;
  };

  VideoTrackRecorder(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      CallbackInterface* callback_interface);

  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame,
                                      base::TimeTicks capture_time) {}
  virtual void OnEncodedVideoFrameForTesting(
      base::TimeTicks now,
      scoped_refptr<EncodedVideoFrame> frame,
      base::TimeTicks capture_time) {}

  CallbackInterface* callback_interface() const { return callback_interface_; }

 protected:
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

 private:
  WeakPersistent<CallbackInterface> const callback_interface_;
};

// VideoTrackRecorderImpl uses the inherited WebMediaStreamSink and encodes the
// video frames received from a Stream Video Track. This class is constructed
// and used on a single thread, namely the main Render thread. This mirrors the
// other MediaStreamVideo* classes that are constructed/configured on Main
// Render thread but that pass frames on Render IO thread. It has an internal
// Encoder with its own threading subtleties, see the implementation file.
class MODULES_EXPORT VideoTrackRecorderImpl : public VideoTrackRecorder {
 public:
  static CodecId GetPreferredCodecId(MediaTrackContainerType type);

  // Returns true if the device has a hardware accelerated encoder which can
  // encode video of the given |width|x|height| and |framerate| to specific
  // |codec|.
  // Note: default framerate value means no restriction.
  static bool CanUseAcceleratedEncoder(CodecId codec,
                                       size_t width,
                                       size_t height,
                                       double framerate = 0.0);

  VideoTrackRecorderImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      CodecProfile codec,
      MediaStreamComponent* track,
      CallbackInterface* callback_interface,
      uint32_t bits_per_second,
      KeyFrameRequestProcessor::Configuration key_frame_config);

  VideoTrackRecorderImpl(const VideoTrackRecorderImpl&) = delete;
  VideoTrackRecorderImpl& operator=(const VideoTrackRecorderImpl&) = delete;

  ~VideoTrackRecorderImpl() override;

  void Pause() override;
  void Resume() override;
  void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame,
                              base::TimeTicks capture_time) override;
  void ForceKeyFrameForNextFrameForTesting();

 private:
  friend class VideoTrackRecorderTest;

  void InitializeEncoder(
      CodecProfile codec,
      const OnEncodedVideoCB& on_encoded_video_cb,
      uint32_t bits_per_second,
      bool allow_vea_encoder,
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks capture_time);
  void InitializeEncoderOnEncoderSupportKnown(
      CodecProfile codec_profile,
      const OnEncodedVideoCB& on_encoded_video_cb,
      uint32_t bits_per_second,
      bool allow_vea_encoder,
      scoped_refptr<media::VideoFrame> frame,
      base::TimeTicks capture_time);
  std::unique_ptr<Encoder> CreateSoftwareVideoEncoder(
      scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
      CodecProfile codec_profile,
      uint32_t bits_per_second,
      bool is_screencast,
      const OnEncodedVideoCB& on_encoded_video_cb);
  std::unique_ptr<Encoder> CreateHardwareVideoEncoder(
      scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
      CodecProfile codec_profile,
      const gfx::Size& input_size,
      uint32_t bits_per_second,
      const OnEncodedVideoCB& on_encoded_video_cb,
      bool use_import_mode,
      bool is_screencast);
  std::unique_ptr<Encoder> CreateMediaVideoEncoder(
      scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
      CodecProfile codec_profile,
      uint32_t bits_per_second,
      bool is_screencast,
      const OnEncodedVideoCB& on_encoded_video_cb,
      bool create_vea_encoder);
  void OnHardwareEncoderError();

  void ConnectToTrack(const VideoCaptureDeliverFrameCB& callback);
  void DisconnectFromTrack();

  // Used to check that we are destroyed on the same sequence we were created.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // We need to hold on to the Blink track to remove ourselves on dtor.
  Persistent<MediaStreamComponent> track_;

  // Holds inner class to encode using whichever codec is configured.
  WTF::SequenceBound<std::unique_ptr<Encoder>> encoder_;

  base::RepeatingCallback<void(
      bool allow_vea_encoder,
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks capture_time)>
      initialize_encoder_cb_;

  bool should_pause_encoder_on_initialization_ = false;
  const KeyFrameRequestProcessor::Configuration key_frame_config_;
  base::WeakPtrFactory<VideoTrackRecorderImpl> weak_factory_{this};
};

// VideoTrackRecorderPassthrough uses the inherited WebMediaStreamSink to
// dispatch EncodedVideoFrame content received from a MediaStreamVideoTrack.
class MODULES_EXPORT VideoTrackRecorderPassthrough : public VideoTrackRecorder {
 public:
  VideoTrackRecorderPassthrough(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      MediaStreamComponent* track,
      CallbackInterface* callback_interface,
      KeyFrameRequestProcessor::Configuration key_frame_config);

  VideoTrackRecorderPassthrough(const VideoTrackRecorderPassthrough&) = delete;
  VideoTrackRecorderPassthrough& operator=(
      const VideoTrackRecorderPassthrough&) = delete;

  ~VideoTrackRecorderPassthrough() override;

  // VideoTrackRecorderBase
  void Pause() override;
  void Resume() override;
  void OnEncodedVideoFrameForTesting(base::TimeTicks now,
                                     scoped_refptr<EncodedVideoFrame> frame,
                                     base::TimeTicks capture_time) override;

 private:
  void RequestKeyFrame();
  void DisconnectFromTrack();
  void HandleEncodedVideoFrame(
      base::RepeatingCallback<base::TimeTicks()> time_now_callback,
      scoped_refptr<EncodedVideoFrame> encoded_frame,
      base::TimeTicks estimated_capture_time);

  // Used to check that we are destroyed on the same sequence we were created.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // This enum class tracks encoded frame waiting and dispatching state. This
  // is needed to guarantee we're dispatching decodable content to
  // |on_encoded_video_cb|. Examples of times where this is needed is
  // startup and Pause/Resume.
  enum class KeyFrameState {
    kWaitingForKeyFrame,
    kKeyFrameReceivedOK,
    kPaused
  };

  // We need to hold on to the Blink track to remove ourselves on dtor.
  const Persistent<MediaStreamComponent> track_;
  KeyFrameState state_ = KeyFrameState::kWaitingForKeyFrame;
  KeyFrameRequestProcessor key_frame_processor_;
  base::WeakPtrFactory<VideoTrackRecorderPassthrough> weak_factory_{this};
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VIDEO_TRACK_RECORDER_H_
