// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/video/fuchsia_video_encode_accelerator.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <stddef.h>
#include <stdint.h>
#include <zircon/errors.h>
#include <zircon/types.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/vmo_buffer.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

// Hardcoded constants defined in the Amlogic driver.
// TODO(crbug.com/42050532): Get this values from platform API rather than
// hardcoding them.
constexpr int kMaxResolutionWidth = 1920;
constexpr int kMaxResolutionHeight = 1088;
constexpr size_t kMaxFrameRate = 60;
constexpr size_t kWidthAlignment = 16;
constexpr size_t kHeightAlignment = 2;
constexpr uint32_t kBytesPerRowAlignment = 32;

// Use 2 buffers for encoder input. Allocating more than one buffers ensures
// that when the decoder is done working on one packet it will have another one
// waiting in the queue. Limiting number of buffers to 2 allows to minimize
// required memory, without significant effect on performance.
constexpr size_t kInputBufferCount = 2;
constexpr uint32_t kOutputBufferCount = 1;

// Allocate 128KiB for SEI/SPS/PPS. (note that the same size is used for all
// codecs, not just H264).
constexpr size_t kOutputFrameConfigSize = 128 * 1024;

const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE,
    // TODO(crbug.com/40241992): Support HEVC codec.
};

fuchsia::sysmem::PixelFormatType GetPixelFormatType(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_I420:
      return fuchsia::sysmem::PixelFormatType::I420;
    case PIXEL_FORMAT_NV12:
      return fuchsia::sysmem::PixelFormatType::NV12;
    default:
      return fuchsia::sysmem::PixelFormatType::INVALID;
  }
}

}  // namespace

// Stores a queue of VideoFrames to be copied to VmoBuffers. VideoFrames can be
// queued before VmoBuffers are available. Queue will not start processing
// before Initialize() is called.
class FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue {
 public:
  using ProcessCB =
      base::RepeatingCallback<void(StreamProcessorHelper::IoPacket)>;

  VideoFrameWriterQueue() = default;

  VideoFrameWriterQueue(const VideoFrameWriterQueue&) = delete;
  VideoFrameWriterQueue& operator=(const VideoFrameWriterQueue&) = delete;

  // Enqueues a VideoFrame. Can be called before `Start()`. Immediately
  // processes `frame` if a VmoBuffer is available. Returns false if the frame
  // is invalid. Returning false will trigger a fatal error, closing the
  // encoder stream and returning an error to the JavaScript layer.
  bool Enqueue(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Initializes the queue and starts processing if possible. `process_cb` is
  // called after each VideoFrame is copied. Returns EncoderStatus::Codes::kOk
  // on success, or an error status if initialization fails (e.g., if buffer
  // settings are invalid or buffers are too small). Returning a failure status
  // will trigger a fatal error, closing the encoder stream and returning an
  // error to the JavaScript layer.
  EncoderStatus Initialize(
      std::vector<VmoBuffer> buffers,
      fuchsia::sysmem2::SingleBufferSettings buffer_settings,
      fuchsia::media::FormatDetails initial_format_details,
      gfx::Size coded_size,
      ProcessCB process_cb);

 private:
  class FrameSize final {
   public:
    static std::optional<FrameSize> Create(
        const fuchsia::sysmem2::ImageFormatConstraints& constraints,
        const gfx::Size& coded_size) {
      if (coded_size.height() % kHeightAlignment != 0) {
        LOG(ERROR) << "Coded height " << coded_size.height()
                   << " is not aligned to " << kHeightAlignment;
        return std::nullopt;
      }
      uint32_t divisor = constraints.bytes_per_row_divisor();
      if (divisor == 0) {
        divisor = 1;
      }
      if (divisor % 2 != 0 && divisor != 1) {
        LOG(ERROR) << "Unsupported odd bytes_per_row_divisor: " << divisor;
        return std::nullopt;
      }
      uint32_t stride = std::max(constraints.min_bytes_per_row(),
                                 static_cast<uint32_t>(coded_size.width()));
      // The `bytes_per_row_divisor` is not guaranteed to be a power of two
      // because it is negotiated as the Least Common Multiple (LCM) across
      // multiple participants in Sysmem (e.g. hardware decoders/encoders,
      // display controllers). For example, if one participant requires a
      // divisor of 4 (power of 2) and another requires 3 (not a power of 2),
      // the negotiated divisor will be 12. Thus, we cannot use
      // `base::bits::AlignUp` here and must use the generic alignment formula.
      uint64_t dst_y_stride =
          ((static_cast<uint64_t>(stride) + divisor - 1) / divisor) * divisor;
      if (dst_y_stride % 2 != 0) {
        LOG(ERROR) << "Calculated destination Y stride is odd: "
                   << dst_y_stride;
        return std::nullopt;
      }
      if (dst_y_stride == 0) {
        LOG(ERROR) << "Failed to calculate destination Y stride (got 0)";
        return std::nullopt;
      }
      if (dst_y_stride > INT32_MAX) {
        LOG(ERROR) << "Destination Y stride overflowed: " << dst_y_stride;
        return std::nullopt;
      }

      // `dst_y_stride` is guaranteed to be even because:
      // 1. If `divisor` is even, any multiple of it (which `dst_y_stride` is
      //    calculated to be) must be even.
      // 2. If `divisor` is 1 (the only allowed odd divisor), we explicitly
      //    validated and rejected odd `dst_y_stride` above.
      CHECK_EQ(dst_y_stride % 2, 0ull);
      uint64_t uv_stride = (dst_y_stride >> 1);
      CHECK_LE(uv_stride, static_cast<uint64_t>(INT32_MAX));

      uint64_t y_plane_size = uint64_t(coded_size.height()) * dst_y_stride;
      if (y_plane_size > INT32_MAX) {
        LOG(ERROR) << "Y plane size calculation overflowed: "
                   << coded_size.height() << " * " << dst_y_stride << " = "
                   << y_plane_size;
        return std::nullopt;
      }

      size_t total_size = y_plane_size;
      total_size += (total_size >> 1);
      if (total_size > INT32_MAX) {
        LOG(ERROR) << "Total size calculation overflowed: " << total_size;
        return std::nullopt;
      }

      return FrameSize(
          coded_size, static_cast<int>(dst_y_stride),
          static_cast<int>(uv_stride), static_cast<int>(y_plane_size),
          static_cast<int>(y_plane_size >> 2), static_cast<int>(total_size));
    }

    gfx::Size coded_size() const { return coded_size_; }
    int dst_y_stride() const { return dst_y_stride_; }
    int dst_uv_stride() const { return dst_uv_stride_; }

    // Guaranteed to be in the range of [0, INT32_MAX], but returned as size_t
    // to avoid static_cast when working with base::span.
    size_t dst_y_plane_size() const {
      return static_cast<size_t>(dst_y_plane_size_);
    }

    // Guaranteed to be in the range of [0, INT32_MAX], but returned as size_t
    // to avoid static_cast when working with base::span.
    size_t dst_uv_plane_size() const {
      return static_cast<size_t>(dst_uv_plane_size_);
    }

    // Guaranteed to be in the range of [0, INT32_MAX], but returned as size_t
    // to avoid static_cast when comparing with buffer sizes.
    size_t dst_size() const { return static_cast<size_t>(dst_size_); }

   private:
    FrameSize(gfx::Size coded_size,
              int dst_y_stride,
              int dst_uv_stride,
              int dst_y_plane_size,
              int dst_uv_plane_size,
              int dst_size)
        : coded_size_(coded_size),
          dst_y_stride_(dst_y_stride),
          dst_uv_stride_(dst_uv_stride),
          dst_y_plane_size_(dst_y_plane_size),
          dst_uv_plane_size_(dst_uv_plane_size),
          dst_size_(dst_size) {}

    const gfx::Size coded_size_;
    const int dst_y_stride_;
    const int dst_uv_stride_;
    const int dst_y_plane_size_;
    const int dst_uv_plane_size_;
    const int dst_size_;
  };

  struct Item {
    Item(scoped_refptr<VideoFrame> frame, bool force_keyframe)
        : frame(std::move(frame)), force_keyframe(force_keyframe) {
      DCHECK(this->frame);
    }

    // Item is move-constructible for popping from the queue.
    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    Item(Item&&) = default;
    Item& operator=(Item&&) = delete;

    scoped_refptr<VideoFrame> frame;
    const bool force_keyframe;
  };

  void ProcessQueue();

  // Marks the VmoBuffer at `buffer_index` to be available for copying.
  void ReleaseBuffer(size_t buffer_index);

  // Copies a VideoFrame from `item` to VmoBuffer at `buffer_index`. Returns
  // true on success. The return value is for testing purposes only; production
  // code always asserts that the copy succeeded.
  bool CopyFrameToBuffer(const Item& item, size_t buffer_index);

  base::queue<Item> queue_;
  std::vector<VmoBuffer> buffers_;
  base::queue<size_t> free_buffer_indices_;
  fuchsia::media::FormatDetails format_details_;
  ProcessCB process_cb_;

  std::optional<FrameSize> frame_size_;

  base::WeakPtrFactory<VideoFrameWriterQueue> weak_factory_{this};
};

// Stores a queue of IoPackets, whose data will be written to BitstreamBuffers.
// Packets can be queued before VmoBuffers are available and before any
// BitstreamBuffers are ready to be used. BitstreamBuffers can become ready
// before VmoBuffers are available. Queue will not start processing before
// Initialize() is called.
class FuchsiaVideoEncodeAccelerator::OutputPacketsQueue {
 public:
  using ProcessCB =
      base::RepeatingCallback<void(int32_t buffer_index,
                                   const BitstreamBufferMetadata& metadata)>;
  using ErrorCB = base::OnceCallback<void(EncoderStatus status)>;

  OutputPacketsQueue() = default;

  OutputPacketsQueue(const OutputPacketsQueue&) = delete;
  OutputPacketsQueue& operator=(const OutputPacketsQueue&) = delete;

  // Initialize the queue and starts processing if possible. `process_cb` is
  // called after data in each IoPacket is copied to BitstreamBuffer.
  void Initialize(std::vector<VmoBuffer> vmo_buffers,
                  ProcessCB process_cb,
                  ErrorCB error_cb);

  // Enqueues an IoPacket. Cannot be called before AcquireVmoBuffers(). Can be
  // called before BitstreamBuffers are ready. Immediately processes `packet` if
  // a BitstreamBuffer is available.
  void Enqueue(StreamProcessorHelper::IoPacket packet);

  // Add an available BitstreamBuffer. Starts processing the next packet in the
  // queue, if exists. Can be called before AcquireVmoBuffers().
  void UseBitstreamBuffer(BitstreamBuffer&& bitstream_buffer);

 private:
  void ProcessQueue();

  // Copies the data stored in VmoBuffer referred by `packet` to a
  // BitstreamBuffer. `metadata` is written with information from `packet`.
  // Returns `true` if no errors occurred.
  bool CopyPacketDataToBitstream(StreamProcessorHelper::IoPacket& packet,
                                 BitstreamBuffer& bitstream_buffer,
                                 BitstreamBufferMetadata* metadata);

  base::queue<StreamProcessorHelper::IoPacket> queue_;
  base::queue<BitstreamBuffer> bitstream_buffers_;
  std::vector<VmoBuffer> vmo_buffers_;
  ProcessCB process_cb_;
  ErrorCB error_cb_;
};

bool FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::Enqueue(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  if (frame->format() != PIXEL_FORMAT_I420) {
    LOG(ERROR) << "Unsupported frame format: " << frame->format();
    return false;
  }

  int width = frame->coded_size().width();
  int height = frame->coded_size().height();
  // Frame dimensions must be positive. We now explicitly disallow 0 dimensions
  // (empty frames), which were previously allowed in production (where DCHECKs
  // are disabled) as no-op copies.
  if (width <= 0 || height <= 0) {
    LOG(ERROR) << "Invalid frame dimensions: " << width << "x" << height;
    return false;
  }

  // YUV 4:2:0 formats (like I420) require height to be aligned to 2 for chroma
  // planes. We must explicitly check the input frame height to match the
  // kHeightAlignment to avoid unexpected out-of-bound write in I420Copy (which
  // uses (height + 1) / 2 for UV rows, whereas we allocate buffer assuming
  // height is aligned).
  if (height % kHeightAlignment != 0) {
    LOG(ERROR) << "Input frame height " << height << " is not aligned to "
               << kHeightAlignment;
    return false;
  }

  for (auto plane :
       {VideoFrame::Plane::kY, VideoFrame::Plane::kU, VideoFrame::Plane::kV}) {
    int stride = frame->stride(plane);
    size_t plane_width = VideoFrame::Columns(plane, PIXEL_FORMAT_I420, width);
    if (stride < 0 || static_cast<size_t>(stride) < plane_width) {
      LOG(ERROR) << "Invalid stride for plane " << plane << ": " << stride
                 << " (minimum: " << plane_width << ")";
      return false;
    }
    size_t rows = VideoFrame::Rows(plane, PIXEL_FORMAT_I420, height);
    if (rows > 0) {
      uint64_t required_size = uint64_t(stride) * (rows - 1) + plane_width;
      if (frame->data_span(plane).size() < required_size) {
        LOG(ERROR) << "Data span size too small for plane " << plane << ": "
                   << frame->data_span(plane).size() << " < " << required_size;
        return false;
      }
    }
  }

  queue_.emplace(std::move(frame), force_keyframe);

  if (!buffers_.empty()) {
    ProcessQueue();
  }
  return true;
}

EncoderStatus FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::Initialize(
    std::vector<VmoBuffer> buffers,
    fuchsia::sysmem2::SingleBufferSettings buffer_settings,
    fuchsia::media::FormatDetails initial_format_details,
    gfx::Size coded_size,
    ProcessCB process_cb) {
  DCHECK(buffers_.empty());
  DCHECK(!buffers.empty());

  buffers_ = std::move(buffers);
  format_details_ = std::move(initial_format_details);
  process_cb_ = std::move(process_cb);

  auto frame_size =
      FrameSize::Create(buffer_settings.image_format_constraints(), coded_size);
  if (!frame_size) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "Failed to calculate frame size"};
  }
  frame_size_.emplace(std::move(*frame_size));

  for (auto& buffer : buffers_) {
    if (frame_size_->dst_size() > buffer.size()) {
      return {
          EncoderStatus::Codes::kEncoderInitializationError,
          base::StringPrintf("VmoBuffer size (%zu) is smaller than required "
                             "destination size (%zu)",
                             buffer.size(), frame_size_->dst_size())};
    }
  }

  // Initially, all buffers are free to use.
  for (size_t i = 0; i < buffers_.size(); i++) {
    free_buffer_indices_.push(i);
  }

  ProcessQueue();
  return EncoderStatus::Codes::kOk;
}

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::ProcessQueue() {
  CHECK(!buffers_.empty());
  CHECK(frame_size_);

  while (!queue_.empty() && !free_buffer_indices_.empty()) {
    Item item = std::move(queue_.front());
    queue_.pop();
    size_t buffer_index = std::move(free_buffer_indices_.front());
    free_buffer_indices_.pop();

    CHECK(CopyFrameToBuffer(item, buffer_index));

    auto packet = StreamProcessorHelper::IoPacket(
        buffer_index, /*offset=*/0, frame_size_->dst_size(),
        item.frame->timestamp(),
        /*unit_end=*/false, /*key_frame=*/false,
        base::BindOnce(&VideoFrameWriterQueue::ReleaseBuffer,
                       weak_factory_.GetWeakPtr(), buffer_index));
    if (item.force_keyframe) {
      fuchsia::media::FormatDetails format_details;
      zx_status_t status = format_details_.Clone(&format_details);
      ZX_DCHECK(status == ZX_OK, status) << "Clone FormatDetails";

      format_details.mutable_encoder_settings()->h264().set_force_key_frame(
          true);
      packet.set_format(std::move(format_details));
    }

    process_cb_.Run(std::move(packet));
  }
}

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::ReleaseBuffer(
    size_t free_buffer_index) {
  DCHECK(!buffers_.empty());

  free_buffer_indices_.push(free_buffer_index);
  ProcessQueue();
}

// Returns false if the frame cannot be copied to the buffer. Although the
// frame parameters are validated in Enqueue() and we expect them to be
// correct here, this function returns a boolean to facilitate testing
// without crashing the process. Production code always asserts the return
// value is true.
bool FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::CopyFrameToBuffer(
    const Item& item,
    size_t buffer_index) {
  if (!frame_size_) [[unlikely]] {
    return false;
  }
  auto& frame = item.frame;
  int width = frame->coded_size().width();
  int height = frame->coded_size().height();

  if (width <= 0 || height <= 0 || width > frame_size_->coded_size().width() ||
      height > frame_size_->coded_size().height()) [[unlikely]] {
    return false;
  }

  if (frame_size_->dst_size() > buffers_[buffer_index].size()) [[unlikely]] {
    return false;
  }

  auto dst_span = buffers_[buffer_index].GetWritableMemory();
  // Strides are guaranteed to be safe by Enqueue validation.

  return libyuv::I420Copy(
             frame->data(VideoFrame::Plane::kY),
             frame->stride(VideoFrame::Plane::kY),
             frame->data(VideoFrame::Plane::kU),
             frame->stride(VideoFrame::Plane::kU),
             frame->data(VideoFrame::Plane::kV),
             frame->stride(VideoFrame::Plane::kV),
             dst_span.first(frame_size_->dst_y_plane_size()).data(),
             frame_size_->dst_y_stride(),
             dst_span
                 .subspan(frame_size_->dst_y_plane_size(),
                          frame_size_->dst_uv_plane_size())
                 .data(),
             frame_size_->dst_uv_stride(),
             dst_span
                 .subspan(frame_size_->dst_y_plane_size() +
                              frame_size_->dst_uv_plane_size(),
                          frame_size_->dst_uv_plane_size())
                 .data(),
             frame_size_->dst_uv_stride(), width, height) == 0;
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::Enqueue(
    StreamProcessorHelper::IoPacket packet) {
  queue_.push(std::move(packet));

  if (!bitstream_buffers_.empty() && !vmo_buffers_.empty()) {
    ProcessQueue();
  }
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::UseBitstreamBuffer(
    BitstreamBuffer&& buffer) {
  bitstream_buffers_.push(std::move(buffer));
  if (!queue_.empty()) {
    ProcessQueue();
  }
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::Initialize(
    std::vector<VmoBuffer> vmo_buffers,
    ProcessCB process_cb,
    ErrorCB error_cb) {
  DCHECK(vmo_buffers_.empty());
  DCHECK(!vmo_buffers.empty());

  vmo_buffers_ = std::move(vmo_buffers);
  process_cb_ = std::move(process_cb);
  error_cb_ = std::move(error_cb);
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::ProcessQueue() {
  DCHECK(!vmo_buffers_.empty());

  while (!queue_.empty() && !bitstream_buffers_.empty()) {
    int bitstream_buffer_id = bitstream_buffers_.front().id();

    BitstreamBufferMetadata metadata;
    bool success = CopyPacketDataToBitstream(
        queue_.front(), bitstream_buffers_.front(), &metadata);
    if (!success)
      return;

    queue_.pop();
    bitstream_buffers_.pop();

    process_cb_.Run(bitstream_buffer_id, metadata);
  }
}

bool FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::
    CopyPacketDataToBitstream(StreamProcessorHelper::IoPacket& packet,
                              BitstreamBuffer& bitstream_buffer,
                              BitstreamBufferMetadata* metadata) {
  if (packet.size() > bitstream_buffer.size()) {
    std::move(error_cb_).Run(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Encoded output is too large. Packet size: %zu "
                            "Bitstream buffer size: %zu",
                            packet.size(), bitstream_buffer.size())});
    return false;
  }

  base::UnsafeSharedMemoryRegion region = bitstream_buffer.TakeRegion();
  base::WritableSharedMemoryMapping mapping =
      region.MapAt(bitstream_buffer.offset(), packet.size());
  if (!mapping.IsValid()) {
    std::move(error_cb_).Run({EncoderStatus::Codes::kSystemAPICallError,
                              "Failed to map BitstreamBuffer memory."});
    return false;
  }

  VmoBuffer& vmo_buffer = vmo_buffers_[packet.buffer_index()];

  metadata->payload_size_bytes =
      vmo_buffer.Read(packet.offset(), mapping.GetMemoryAsSpan<uint8_t>());
  metadata->key_frame = packet.key_frame();
  metadata->timestamp = packet.timestamp();
  return true;
}

FuchsiaVideoEncodeAccelerator::FuchsiaVideoEncodeAccelerator()
    : sysmem_allocator_("CrFuchsiaHWVideoEncoder") {}

FuchsiaVideoEncodeAccelerator::~FuchsiaVideoEncodeAccelerator() {
  DCHECK(!encoder_);
}

VideoEncodeAccelerator::SupportedProfiles
FuchsiaVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;

  SupportedProfile profile;
  profile.max_framerate_numerator = kMaxFrameRate;
  profile.max_framerate_denominator = 1;
  profile.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                               VideoEncodeAccelerator::kVariableMode;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  for (const auto& supported_profile : kSupportedProfiles) {
    profile.profile = supported_profile;
    profiles.push_back(profile);
  }
  return profiles;
}

EncoderStatus FuchsiaVideoEncodeAccelerator::Initialize(
    const VideoEncodeAccelerator::Config& config,
    VideoEncodeAccelerator::Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int width = config.input_visible_size.width(),
      height = config.input_visible_size.height();
  if (width % kWidthAlignment != 0 || height % kHeightAlignment != 0) {
    MEDIA_LOG(ERROR, media_log)
        << "Fuchsia MediaCodec is only tested with resolutions that have width "
           "alignment "
        << kWidthAlignment << " and height alignment " << kHeightAlignment;
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (width <= 0 || height <= 0) {
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }
  if (width > kMaxResolutionWidth || height > kMaxResolutionHeight) {
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  // TODO(crbug.com/40241991): Support NV12 pixel format.
  if (config.input_format != PIXEL_FORMAT_I420) {
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }
  // TODO(crbug.com/40241992): Support HEVC codec.
  if (config.output_profile != H264PROFILE_BASELINE) {
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  vea_client_ = client;
  media_log_ = std::move(media_log);
  config_ = std::make_unique<Config>(config);

  input_queue_ = std::make_unique<VideoFrameWriterQueue>();
  output_queue_ = std::make_unique<OutputPacketsQueue>();

  fuchsia::mediacodec::CodecFactoryPtr codec_factory =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::mediacodec::CodecFactory>();

  fuchsia::mediacodec::CreateEncoder_Params encoder_params;
  encoder_params.set_require_hw(true);
  encoder_params.set_input_details(CreateFormatDetails(*config_));

  fuchsia::media::StreamProcessorPtr stream_processor;
  codec_factory->CreateEncoder(std::move(encoder_params),
                               stream_processor.NewRequest());
  encoder_ = std::make_unique<StreamProcessorHelper>(
      std::move(stream_processor), this);

  // Output buffer size is calculated based on the input size with MinCR of 2,
  // plus config size.
  size_t allocation_size = VideoFrame::AllocationSize(
      config.input_format, config_->input_visible_size);
  auto output_buffer_size = allocation_size / 2 + kOutputFrameConfigSize;

  vea_client_->RequireBitstreamBuffers(
      /*input_count=*/1, /*input_coded_size=*/config_->input_visible_size,
      output_buffer_size);
  return {EncoderStatus::Codes::kOk};
}

void FuchsiaVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_queue_->UseBitstreamBuffer(std::move(buffer));
}

void FuchsiaVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                           bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config_);
  DCHECK_EQ(frame->format(), PIXEL_FORMAT_I420);
  DCHECK(!frame->coded_size().IsEmpty());
  CHECK(frame->HasDirectCpuAccess());

  // Fuchsia VEA ignores the frame's `visible_rect` and encodes the whole
  // `coded_size`. So we need to check that `coded_size` fits in the allocated
  // buffer based on `input_visible_size`. This check should not fail due to
  // the frame's alignment, as `input_visible_size.width()` must be aligned to
  // `kWidthAlignment`.
  //
  // TODO(crbug.com/40245141): Encode only the `visible_rect` of a frame.
  if (frame->coded_size().width() > config_->input_visible_size.width() ||
      frame->coded_size().height() > config_->input_visible_size.height()) {
    OnError({EncoderStatus::Codes::kInvalidInputFrame,
             base::StringPrintf(
                 "Input frame size %s is larger than configured size %s",
                 frame->coded_size().ToString().c_str(),
                 config_->input_visible_size.ToString().c_str())});
    return;
  }

  if (!input_queue_->Enqueue(std::move(frame), force_keyframe)) {
    OnError({EncoderStatus::Codes::kInvalidInputFrame, "Invalid input frame"});
  }
}

void FuchsiaVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  // TODO(crbug.com/40241995): Implement RequestEncodingParameterChange.
  NOTIMPLEMENTED();
}

void FuchsiaVideoEncodeAccelerator::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReleaseEncoder();
  delete this;
}

bool FuchsiaVideoEncodeAccelerator::IsFlushSupported() {
  // TODO(crbug.com/40242985): Implement Flush.
  return false;
}

bool FuchsiaVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return false;
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorAllocateInputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  input_buffer_collection_ = sysmem_allocator_.AllocateNewCollection();
  input_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::SetInputBufferCollectionToken,
                     base::Unretained(encoder_.get())));

  fuchsia::sysmem2::BufferCollectionConstraints constraints =
      VmoBuffer::GetRecommendedConstraints(kInputBufferCount,
                                           /*min_buffer_size=*/std::nullopt,
                                           /*writable=*/true);
  input_buffer_collection_->Initialize(std::move(constraints),
                                       "VideoEncoderInput");
  input_buffer_collection_->AcquireBuffers(
      base::BindOnce(&FuchsiaVideoEncodeAccelerator::OnInputBuffersAcquired,
                     base::Unretained(this)));
}

void FuchsiaVideoEncodeAccelerator::OnInputBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings& buffer_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config_);

  const auto& image_constraints = buffer_settings.image_format_constraints();
  int coded_width =
      base::bits::AlignUp(std::max(image_constraints.min_size().width,
                                   image_constraints.required_max_size().width),
                          image_constraints.size_alignment().width);
  int coded_height = base::bits::AlignUp(
      std::max(image_constraints.min_size().height,
               image_constraints.required_max_size().height),
      image_constraints.size_alignment().height);
  CHECK_GE(coded_width, config_->input_visible_size.width());
  CHECK_GE(coded_height, config_->input_visible_size.height());

  EncoderStatus status = input_queue_->Initialize(
      std::move(buffers), fidl::Clone(buffer_settings),
      CreateFormatDetails(*config_), gfx::Size(coded_width, coded_height),
      base::BindRepeating(&StreamProcessorHelper::Process,
                          base::Unretained(encoder_.get())));
  if (!status.is_ok()) {
    OnError(status);
  }
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorAllocateOutputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_buffer_collection_ = sysmem_allocator_.AllocateNewCollection();
  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::CompleteOutputBuffersAllocation,
                     base::Unretained(encoder_.get())));

  fuchsia::sysmem2::BufferCollectionConstraints constraints;
  constraints.mutable_usage()->set_cpu(fuchsia::sysmem2::CPU_USAGE_READ);
  constraints.set_min_buffer_count_for_shared_slack(kOutputBufferCount);
  output_buffer_collection_->Initialize(std::move(constraints),
                                        "VideoEncoderOutput");
  output_buffer_collection_->AcquireBuffers(
      base::BindOnce(&FuchsiaVideoEncodeAccelerator::OnOutputBuffersAcquired,
                     base::Unretained(this)));
}

void FuchsiaVideoEncodeAccelerator::OnOutputBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings& buffer_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_queue_->Initialize(
      std::move(buffers),
      base::BindRepeating(&VideoEncodeAccelerator::Client::BitstreamBufferReady,
                          base::Unretained(vea_client_.get())),
      base::BindOnce(&FuchsiaVideoEncodeAccelerator::OnError,
                     base::Unretained(this)));
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorOutputFormat(
    fuchsia::media::StreamOutputFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* format_details = format.mutable_format_details();
  if (!format_details->has_domain() || !format_details->domain().is_video() ||
      !format_details->domain().video().is_compressed()) {
    OnError({EncoderStatus::Codes::kEncoderFailedEncode,
             "Received invalid format from stream processor."});
  }
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorEndOfStream() {
  // StreamProcessor should not return EoS when Flush is not supported.
  NOTIMPLEMENTED();
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorOutputPacket(
    StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_queue_->Enqueue(std::move(packet));
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorNoKey() {
  // This method is only used for decryption.
  NOTREACHED();
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnError({EncoderStatus::Codes::kEncoderFailedEncode,
           "Encountered stream processor error."});
}

void FuchsiaVideoEncodeAccelerator::ReleaseEncoder() {
  // Drop queues and buffers before encoder, as their callbacks can reference
  // the encoder.
  input_queue_.reset();
  output_queue_.reset();
  input_buffer_collection_.reset();
  output_buffer_collection_.reset();

  encoder_.reset();
}

void FuchsiaVideoEncodeAccelerator::OnError(EncoderStatus status) {
  CHECK(!status.is_ok());
  LOG(ERROR) << "FuchsiaVideoEncodeAccelerator failed, error_code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (media_log_) {
    MEDIA_LOG(ERROR, media_log_) << status.message();
  }
  ReleaseEncoder();
  if (vea_client_) {
    vea_client_->NotifyErrorStatus(status);
  }
}

fuchsia::media::FormatDetails
FuchsiaVideoEncodeAccelerator::CreateFormatDetails(
    VideoEncodeAccelerator::Config& config) {
  DCHECK(config.input_visible_size.width() > 0);
  DCHECK(config.input_visible_size.height() > 0);

  uint32_t width = static_cast<uint32_t>(config.input_visible_size.width()),
           height = static_cast<uint32_t>(config.input_visible_size.height());

  DCHECK(width % kWidthAlignment == 0);
  DCHECK(height % kHeightAlignment == 0);

  fuchsia::media::FormatDetails format_details;
  format_details.set_format_details_version_ordinal(1);

  fuchsia::media::VideoUncompressedFormat uncompressed;
  uncompressed.image_format = fuchsia::sysmem::ImageFormat_2{
      .pixel_format = fuchsia::sysmem::PixelFormat{.type = GetPixelFormatType(
                                                       config.input_format)},
      .coded_width = width,
      .coded_height = height,
      // TODO(crbug.com/518074543): We may want to revise the use of a fixed row
      // alignment.
      .bytes_per_row = base::bits::AlignUp(width, kBytesPerRowAlignment),
      .display_width = width,
      .display_height = height,
  };
  fuchsia::media::VideoFormat video_format;
  video_format.set_uncompressed(std::move(uncompressed));
  fuchsia::media::DomainFormat domain;
  domain.set_video(std::move(video_format));
  format_details.set_domain(std::move(domain));

  // For now, hardcode mime type for H264.
  // TODO(crbug.com/40241992): Support HEVC codec.
  DCHECK(config.output_profile == H264PROFILE_BASELINE);
  format_details.set_mime_type("video/h264");
  fuchsia::media::H264EncoderSettings h264_settings;
  if (config.bitrate.target_bps() != 0) {
    h264_settings.set_bit_rate(config.bitrate.target_bps());
  }
  h264_settings.set_frame_rate(config.framerate);
  format_details.set_timebase(base::Time::kNanosecondsPerSecond /
                              config.framerate);

  if (config.gop_length.has_value()) {
    h264_settings.set_gop_size(config.gop_length.value());
  }
  h264_settings.set_force_key_frame(false);

  fuchsia::media::EncoderSettings encoder_settings;
  encoder_settings.set_h264(std::move(h264_settings));
  format_details.set_encoder_settings(std::move(encoder_settings));

  return format_details;
}

}  // namespace media
