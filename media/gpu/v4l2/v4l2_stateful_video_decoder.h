// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_STATEFUL_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_V4L2_STATEFUL_VIDEO_DECODER_H_

#include <linux/videodev2.h>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class Location;
class SequencedTaskRunner;
}  // namespace base

namespace media {

class V4L2Queue;

// V4L2StatefulVideoDecoder is an implementation of VideoDecoderMixin
// (an augmented media::VideoDecoder) for stateful V4L2 decoding drivers
// (e.g. those in ChromeOS Qualcomm devices, and Mediatek 8173). This API has
// changed along the kernel versions, but a given copy can be found in [1]
// (the most up-to-date is in [2]).
//
// This class operates on a single thread, where it is constructed and
// destroyed.
//
// [1]
// https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/dev-decoder.html
// [2]
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-decoder.html
class MEDIA_GPU_EXPORT V4L2StatefulVideoDecoder : public VideoDecoderMixin {
 public:
  static std::unique_ptr<VideoDecoderMixin> Create(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  static absl::optional<SupportedVideoDecoderConfigs> GetSupportedConfigs();

  // VideoDecoderMixin implementation, VideoDecoder part.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  // VideoDecoderMixin implementation, specific part.
  void ApplyResolutionChange() override;
  size_t GetMaxOutputFramePoolSize() const override;
  void SetDmaIncoherentV4L2(bool incoherent) override;

 private:
  V4L2StatefulVideoDecoder(std::unique_ptr<MediaLog> media_log,
                           scoped_refptr<base::SequencedTaskRunner> task_runner,
                           base::WeakPtr<VideoDecoderMixin::Client> client);
  ~V4L2StatefulVideoDecoder() override;

  // Tries to create, configure and fill |CAPTURE_queue_|. This method, which
  // should be called after PollOnceForResolutionChangeEvent() has returned
  // true, queries the native |CAPTURE_queue_| configuration and supported
  // capabilities and negotiates the preferred configuration with our |client_|
  // (via PickDecoderOutputFormat()). It then tries to configure, stream on,
  // and fill in said |CAPTURE_queue_|. Returns false if any step goes wrong,
  // in particular any ioctl() call.
  bool InitializeCAPTUREQueue();

  // Before |CAPTURE_queue_| is to be configured, we need to ask our |client_|
  // (usually VideoDecoderPipeline) to PickDecoderOutputFormat(), for which we
  // provide some candidates. This method enumerates such candidates, or returns
  // an empty vector.
  std::vector<ImageProcessor::PixelLayoutCandidate>
  EnumeratePixelLayoutCandidates(const gfx::Size& coded_size);

  // Estimates the number of buffers needed for the |CAPTURE_queue_| and for
  // codec reference requirements.This function cannot fail (at least returns a
  // default, conservative value).
  size_t GetNumberOfReferenceFrames();

  // Convenience method to PostTask a wait for a |CAPTURE_queue_| event with
  // callbacks pointing to TryAndDequeueCAPTUREQueueBuffers() (for data
  // available) and InitializeCAPTUREQueue() (for re/configuration events).
  void RearmCAPTUREQueueMonitoring();
  // Dequeues all the available |CAPTURE_queue_| buffers and sends their
  // associated VideoFrames to |output_cb_|. If all goes well, it will
  // RearmCAPTUREQueueMonitoring().
  // TODO(mcasas): Currently we also TryAndEnqueueCAPTUREQueueBuffers(), is this
  // a good spot for that?
  void TryAndDequeueCAPTUREQueueBuffers();

  // Tries to "enqueue" all available |CAPTURE_queue_| buffers in the driver's
  // CAPTURE queue (V4L2Queues don't do that by default upon allocation).
  void TryAndEnqueueCAPTUREQueueBuffers();

  // Dequeues all the available |OUTPUT_queue_| buffers. This will effectively
  // make those available for sending further encoded chunks to the driver.
  // Returns false if any ioctl fails, true otherwise.
  bool DrainOUTPUTQueue();

  // Tries to "enqueue" all encoded chunks in |decoder_buffer_and_callbacks_|
  // in |OUTPUT_queue_|, Run()nning their respective DecodeCBs. Returns false if
  // any enqueueing operation's ioctl fails, true otherwise.
  bool TryAndEnqueueOUTPUTQueueBuffers();

  // Prints a VLOG with the state of |OUTPUT_queue| and |CAPTURE_queue_| for
  // debugging, preceded with |from_here|s function name.
  void PrintOutQueueStatesForVLOG(const base::Location& from_here);

  base::ScopedFD device_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  // This |wake_event_| is used to interrupt a blocking poll() call, such as the
  // one started by e.g. RearmCAPTUREQueueMonitoring().
  base::ScopedFD wake_event_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Bitstream information and other stuff collected during Initialize().
  VideoCodecProfile profile_ GUARDED_BY_CONTEXT(sequence_checker_) =
      VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoAspectRatio aspect_ratio_ GUARDED_BY_CONTEXT(sequence_checker_);
  OutputCB output_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  DecodeCB flush_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds pairs of encoded chunk (DecoderBuffer) and associated DecodeCB for
  // decoding via TryAndEnqueueOUTPUTQueueBuffers().
  base::queue<std::pair<scoped_refptr<DecoderBuffer>, DecodeCB>>
      decoder_buffer_and_callbacks_;

  // OUTPUT in V4L2 terminology is the queue holding encoded chunks of
  // bitstream. CAPTURE is the queue holding decoded pictures. See e.g. [1].
  // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/dev-decoder.html#glossary
  scoped_refptr<V4L2Queue> OUTPUT_queue_ GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<V4L2Queue> CAPTURE_queue_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A sequenced TaskRunner to wait for events coming from |CAPTURE_queue_| or
  // |wake_event_|.
  scoped_refptr<base::SequencedTaskRunner> event_task_runner_;
  // Used to (try to) cancel the Tasks sent by RearmCAPTUREQueueMonitoring(),
  // and not serviced yet, when no longer needed.
  base::CancelableTaskTracker cancelable_task_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Pegged to the construction and main work thread. Notably, |task_runner| is
  // not used.
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer/factory associated with the main thread (|sequence_checker|).
  base::WeakPtr<V4L2StatefulVideoDecoder> weak_this_;
  base::WeakPtrFactory<V4L2StatefulVideoDecoder> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_STATEFUL_VIDEO_DECODER_H_
