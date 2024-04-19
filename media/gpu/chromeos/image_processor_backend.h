// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_BACKEND_H_
#define MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_BACKEND_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_GPU_EXPORT ImageProcessorBackend {
 public:
  // Callback for returning a processed image to the client.
  using FrameReadyCB = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;
  // Callback for returning a processed image to the client.
  // Used when calling the "legacy" Process() method with buffers that are
  // managed by the processor. The first argument is the index of the returned
  // buffer.
  using LegacyFrameReadyCB =
      base::OnceCallback<void(size_t, scoped_refptr<VideoFrame>)>;

  // Callback for notifying client when error occurs.
  using ErrorCB = base::RepeatingClosure;

  // OutputMode is used as intermediate stage. The ultimate goal is to make
  // ImageProcessor's clients all use IMPORT output mode.
  // TODO(crbug.com/907767): Remove this once ImageProcessor always works as
  // IMPORT mode for output.
  enum class OutputMode { ALLOCATE, IMPORT };

  // Encapsulates ImageProcessor input / output configurations.
  struct MEDIA_GPU_EXPORT PortConfig {
    PortConfig() = delete;
    PortConfig(const PortConfig&);
    PortConfig(
        Fourcc fourcc,
        const gfx::Size& size,
        const std::vector<ColorPlaneLayout>& planes,
        const gfx::Rect& visible_rect,
        const std::vector<VideoFrame::StorageType>& preferred_storage_types);
    ~PortConfig();

    bool operator==(const PortConfig& other) const {
      return fourcc == other.fourcc && size == other.size &&
             planes == other.planes && visible_rect == other.visible_rect &&
             preferred_storage_types == other.preferred_storage_types;
    }
    bool operator!=(const PortConfig& other) const { return !(*this == other); }

    // Get the first |preferred_storage_types|.
    // If |preferred_storage_types| is empty, return STORAGE_UNKNOWN.
    VideoFrame::StorageType storage_type() const {
      return preferred_storage_types.empty() ? VideoFrame::STORAGE_UNKNOWN
                                             : preferred_storage_types.front();
    }

    // Output human readable string of PortConfig.
    // Example:
    // PortConfig(format::NV12, size:640x480, planes:[(640, 0, 307200),
    // (640,0,153600)], visible_rect:0, 0, 640x480, storage_types:[DMABUFS])
    std::string ToString() const;

    // Video frame format represented as fourcc type.
    const Fourcc fourcc;

    // Width and height of the video frame in pixels. This must include pixel
    // data for the whole image; i.e. for YUV formats with subsampled chroma
    // planes. If a visible portion of the image does not line up on a sample
    // boundary, |size_| must be rounded up appropriately.
    const gfx::Size size;

    // Layout property (stride, offset, size of bytes) for each color plane.
    const std::vector<ColorPlaneLayout> planes;
    const gfx::Rect visible_rect;
    // List of preferred storage types.
    const std::vector<VideoFrame::StorageType> preferred_storage_types;
  };

  // Process |input_frame| and store in |output_frame|. Only used when output
  // mode is IMPORT. After processing, call |cb| with |output_frame|.
  virtual void Process(scoped_refptr<VideoFrame> input_frame,
                       scoped_refptr<VideoFrame> output_frame,
                       FrameReadyCB cb) = 0;

  // Process |frame| and store in in a ImageProcessor-owned output buffer. Only
  // used when output mode is ALLOCATE. After processing, call |cb| with the
  // buffer.
  // If ALLOCATE mode is not supported, the implementation is optional. In this
  // case, this method should not be called and the default implementation will
  // panic.
  virtual void ProcessLegacy(scoped_refptr<VideoFrame> frame,
                             LegacyFrameReadyCB cb);

  // Drop all pending process requests. The default implementation is no-op.
  virtual void Reset();

  // Getter methods of data members, which could be called on any sequence.
  const PortConfig& input_config() const { return input_config_; }
  const PortConfig& output_config() const { return output_config_; }
  OutputMode output_mode() const { return output_mode_; }

  virtual bool needs_linear_output_buffers() const;

  virtual bool supports_incoherent_buffers() const;

  const scoped_refptr<base::SequencedTaskRunner>& task_runner() const {
    return backend_task_runner_;
  }

  virtual std::string type() const = 0;

 protected:
  friend struct std::default_delete<ImageProcessorBackend>;

  ImageProcessorBackend(
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      VideoRotation relative_rotation,
      ErrorCB error_cb,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  virtual ~ImageProcessorBackend();
  virtual void Destroy();

  const PortConfig input_config_;
  const PortConfig output_config_;

  // TODO(crbug.com/907767): Remove |output_mode_| once ImageProcessor always
  // works as IMPORT mode for output.
  const OutputMode output_mode_;

  // ImageProcessor performs a rotation if the |relative_rotation_| is not equal
  // to VIDEO_ROTATION_0.
  const VideoRotation relative_rotation_;

  // Call this callback when any error occurs.
  const ErrorCB error_cb_;

  // The main sequence and its checker. Except getter methods, all public
  // methods and callbacks are called on this sequence. The proper
  // SequencedTaskRunner is created by ImageProcessorBackend.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  SEQUENCE_CHECKER(backend_sequence_checker_);
};

}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct MEDIA_GPU_EXPORT default_delete<media::ImageProcessorBackend> {
  constexpr default_delete() = default;

  template <
      typename U,
      typename = typename std::enable_if<
          std::is_convertible<U*, media::ImageProcessorBackend*>::value>::type>
  default_delete(const default_delete<U>& d) {}

  void operator()(media::ImageProcessorBackend* ptr) const;
};

}  // namespace std

#endif  // MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_BACKEND_H_
