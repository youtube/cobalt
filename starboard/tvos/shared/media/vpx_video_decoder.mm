// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/tvos/shared/media/vpx_video_decoder.h"

#import <OpenGLES/ES3/gl.h>

#include <map>

#include "starboard/common/string.h"
#include "starboard/shared/starboard/decode_target/decode_target_context_runner.h"
#include "starboard/system.h"
#import "starboard/tvos/shared/media/egl_adapter.h"
#include "starboard/tvos/shared/starboard_application.h"

namespace starboard {
namespace shared {
namespace uikit {
namespace {

const int kMaxNumberOfPlanes = 3;

using starboard::player::JobThread;
using starboard::player::filter::VideoFrame;
using ::starboard::shared::starboard::decode_target::DecodeTargetContextRunner;

class VideoFrameImpl : public VideoFrame {
 public:
  VideoFrameImpl(int64_t presentation_time,
                 const std::function<void()>& destroy_frame_cb)
      : VideoFrame(presentation_time), destroy_frame_cb_(destroy_frame_cb) {
    SB_DCHECK(destroy_frame_cb_);
  }
  ~VideoFrameImpl() override { destroy_frame_cb_(); }

 private:
  const std::function<void()> destroy_frame_cb_;
};

bool IsAppleTV5G() {
  char model_name[128];
  bool succeeded = SbSystemGetProperty(kSbSystemPropertyModelName, model_name,
                                       sizeof(model_name));
  SB_DCHECK(succeeded);
  return succeeded && strcmp(model_name, "AppleTV6-2") == 0;
}

}  // namespace

class VpxVideoDecoder::TextureProvider
    : public RefCountedThreadSafe<TextureProvider> {
 public:
  explicit TextureProvider(SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider)
      : decode_target_graphics_context_provider_(
            decode_target_graphics_context_provider) {}

  void GetOpenGLTextures(SbDecodeTargetFormat format,
                         int width,
                         int height,
                         int stride,
                         unsigned char* const* planes,
                         GLuint* textures) {
    // This function should be called only on texture thread.
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK([EAGLContext currentContext]);
    SB_DCHECK(textures);
    // Only support kSbDecodeTargetFormat3PlaneYUVI420 now.
    SB_DCHECK(format == kSbDecodeTargetFormat3PlaneYUVI420);
    SB_DCHECK(format_ == kSbDecodeTargetFormatInvalid ||
              (format_ == format && width == width_ && height == height_));

    if (format_ == kSbDecodeTargetFormatInvalid) {
      format_ = format;
      width_ = width;
      height_ = height;
    }
    GLuint* ret_textures = nullptr;
    bool is_new_textures = false;
    int num_planes = kMaxNumberOfPlanes;
    // Gen textures.
    {
      ScopedLock lock(mutex_);
      if (!freed_textures_.empty()) {
        ret_textures = freed_textures_.front();
        freed_textures_.pop_front();
      } else {
        ret_textures = new GLuint[3];
        glGenTextures(num_planes, ret_textures);
        textures_.insert(std::make_pair(ret_textures[0], ret_textures));
        is_new_textures = true;
      }
    }
    // Upload textures.
    for (int i = 0; i < num_planes; i++) {
      int texture_width = i == 0 ? width : width / 2;
      int texture_height = i == 0 ? height : height / 2;
      int texture_stride = i == 0 ? stride : stride / 2;

      glBindTexture(GL_TEXTURE_2D, ret_textures[i]);
      SB_DCHECK(glGetError() == GL_NO_ERROR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, texture_stride);
      if (is_new_textures) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texture_width, texture_height,
                     0, GL_ALPHA, GL_UNSIGNED_BYTE, planes[i]);
      } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture_width, texture_height,
                        GL_ALPHA, GL_UNSIGNED_BYTE, planes[i]);
      }
      SB_DCHECK(glGetError() == GL_NO_ERROR);
    }
    // Restore |GL_UNPACK_ROW_LENGTH|
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    memcpy(textures, ret_textures, num_planes * sizeof(GLuint));
  }

  void RecycleTextures(GLuint* textures) {
    SB_DCHECK(textures);

    ScopedLock lock(mutex_);
    auto it = textures_.find(textures[0]);
    SB_DCHECK(it != textures_.end());
    SB_DCHECK(std::find(freed_textures_.begin(), freed_textures_.end(),
                        it->second) == freed_textures_.end());
    freed_textures_.push_back(it->second);
  }

 protected:
  friend class RefCountedThreadSafe<TextureProvider>;
  ~TextureProvider() {
    // TextureProvider may be released on renderer thread, texture thread or
    // player worker thread. If released on player worker thread, as there's no
    // available egal context, have to delete textures on renderer thread. Pay
    // attention to avoid deadlock.
    if ([EAGLContext currentContext]) {
      DeleteAllTextures();
    } else {
      // Call DeleteTextures on renderer thread.
      DecodeTargetContextRunner(decode_target_graphics_context_provider_)
          .RunOnGlesContext(
              std::bind(&TextureProvider::DeleteAllTextures, this));
    }
  }

  void DeleteAllTextures() {
    SB_DCHECK(textures_.size() == freed_textures_.size());
    for (auto it = textures_.begin(); it != textures_.end(); it++) {
      glDeleteTextures(kMaxNumberOfPlanes, it->second);
      SB_DCHECK(glGetError() == GL_NO_ERROR);
      delete[] it->second;
    }
  }

  starboard::ThreadChecker thread_checker_;
  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;
  SbDecodeTargetFormat format_ = kSbDecodeTargetFormatInvalid;
  int width_ = 0;
  int height_ = 0;
  Mutex mutex_;
  std::map<GLuint, GLuint*> textures_;
  std::list<GLuint*> freed_textures_;
};

class VpxVideoDecoder::DecodedImageData {
 public:
  DecodedImageData(int64_t timestamp, const vpx_image_t* vpx_image)
      : timestamp_(timestamp) {
    SB_DCHECK(vpx_image);
    SB_DCHECK(vpx_image->fb_priv);

    SB_DCHECK(vpx_image->fmt == VPX_IMG_FMT_I420);

    width_ = vpx_image->d_w;
    height_ = vpx_image->d_h;
    stride_ = vpx_image->w;
    planes_[0] = vpx_image->planes[0];
    planes_[1] = vpx_image->planes[1];
    planes_[2] = vpx_image->planes[2];

    hw_image_ = vpx_image->fb_priv;
    decoder_acquire_hw_image(hw_image_);
  }

  ~DecodedImageData() { decoder_release_hw_image(hw_image_); }

  SbDecodeTargetFormat format() const { return format_; }
  int num_planes() const { return num_planes_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int stride() const { return stride_; }
  unsigned char* const* planes() const { return planes_; }
  int64_t timestamp() const { return timestamp_; }

 private:
  DecodedImageData(const DecodedImageData&) = delete;
  DecodedImageData& operator=(const DecodedImageData&) = delete;

  SbDecodeTargetFormat format_ = kSbDecodeTargetFormat3PlaneYUVI420;
  int num_planes_ = kMaxNumberOfPlanes;
  int width_ = 0;
  int height_ = 0;
  int stride_ = 0;
  unsigned char* planes_[kMaxNumberOfPlanes];
  int64_t timestamp_ = 0;
  void* hw_image_ = NULL;
};

class VpxVideoDecoder::DecodeTargetData : public SbDecodeTargetPrivate::Data {
 public:
  DecodeTargetData(scoped_refptr<TextureProvider> texture_provider,
                   const VpxVideoDecoder::DecodedImageData& data)
      : texture_provider_(texture_provider), timestamp_(data.timestamp()) {
    SB_DCHECK(data.format() == kSbDecodeTargetFormat3PlaneYUVI420);
    SB_DCHECK(texture_provider_);
    SB_DCHECK([EAGLContext currentContext]);

    num_planes_ = data.num_planes();
    info_.format = data.format();
    info_.is_opaque = true;
    info_.width = data.width();
    info_.height = data.height();

    texture_provider_->GetOpenGLTextures(info_.format, info_.width,
                                         info_.height, data.stride(),
                                         data.planes(), textures_);

    for (int i = 0; i < num_planes_; i++) {
      info_.planes[i].width = i == 0 ? info_.width : info_.width / 2;
      info_.planes[i].height = i == 0 ? info_.height : info_.height / 2;
      info_.planes[i].content_region.left = 0;
      info_.planes[i].content_region.top = 0;
      info_.planes[i].content_region.right = info_.planes[i].width;
      info_.planes[i].content_region.bottom = info_.planes[i].height;
      info_.planes[i].texture = textures_[i];
      info_.planes[i].gl_texture_target = GL_TEXTURE_2D;
      info_.planes[i].gl_texture_format = GL_ALPHA;
    }
    // Restore |GL_UNPACK_ROW_LENGTH|
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }

  ~DecodeTargetData() {
    texture_provider_->RecycleTextures(textures_);
    // Explicitly release |texture_provider_| here as a reminder,
    // |texture_provider_| is ref counted, release |texture_provider_| may
    // invoke |texture_provider_|'s destructor, which may release texture
    // on renderer thread. Pay attention to the deadlock.
    texture_provider_ = nullptr;
  }

  const SbDecodeTargetInfo& info() const override { return info_; }

  int64_t timestamp() const { return timestamp_; }

 private:
  SbDecodeTargetInfo info_;
  scoped_refptr<TextureProvider> texture_provider_;
  int num_planes_;
  GLuint textures_[kMaxNumberOfPlanes];
  int64_t timestamp_;
};

VpxVideoDecoder::VpxVideoDecoder(SbPlayerOutputMode output_mode,
                                 SbDecodeTargetGraphicsContextProvider*
                                     decode_target_graphics_context_provider)
    : decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      loop_filter_manager_(Is4KSupported(),
                           kMinimumPrerollFrameCount,
                           kDisableLoopFilterLowWatermark,
                           kDisableLoopFilterHighWatermark) {
  SB_DCHECK(output_mode == kSbPlayerOutputModeDecodeToTexture);
}

VpxVideoDecoder::~VpxVideoDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  Reset();
  // Explicitly release |current_decode_target_data_| here as a reminder, it may
  // release texture on renderer thread. Pay attention to the deadlock.
  current_decode_target_data_ = nullptr;
}

// static
bool VpxVideoDecoder::Is4KSupported() {
  static bool is_4k_supported = IsAppleTV5G();
  return is_4k_supported;
}

void VpxVideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                 const ErrorCB& error_cb) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

size_t VpxVideoDecoder::GetPrerollFrameCount() const {
  return loop_filter_manager_.GetPrerollFrameCount();
}

void VpxVideoDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_status_cb_);

  if (error_occurred_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after an error.";
    return;
  }
  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("vpx_video_decoder"));
    decoder_thread_->job_queue()->Schedule(
        std::bind(&VpxVideoDecoder::DecodeOneBuffer, this));
  }

  const auto& input_buffer = input_buffers[0];

  ScopedLock lock(pending_buffers_mutex_);
  pending_buffers_total_size_in_bytes_ += input_buffer->size();
  pending_buffers_.push_back(input_buffer);
  if (pending_buffers_.size() < kMaxNumberOfCachedInputBuffer) {
    decoder_status_cb_(kNeedMoreInput, NULL);
  }
}

void VpxVideoDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(decoder_status_cb_);

  if (error_occurred_) {
    SB_LOG(ERROR) << "WriteEndOfStream() was called after an error.";
    return;
  }

  // We have to flush the decoder to decode the rest frames and to ensure that
  // Decode() is not called when the stream is ended.
  stream_ended_ = true;

  if (!decoder_thread_) {
    // In case there is no WriteInputBuffer() call before WriteEndOfStream(),
    // don't create the decoder thread and send the EOS frame directly.
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }

  decoder_thread_->job_queue()->Schedule(
      std::bind(&VpxVideoDecoder::DecodeEndOfStream, this));
}

void VpxVideoDecoder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_thread_) {
    // Run TeardownCodec on |decoder_thread_|.
    decoder_thread_->job_queue()->Schedule(
        std::bind(&VpxVideoDecoder::TeardownCodec, this, true));
    // Wait until all tasks are finished.
    decoder_thread_.reset();
  }

  pending_buffers_.clear();
  pending_buffers_total_size_in_bytes_ = 0;

  decoder_status_cb_(kReleaseAllFrames, nullptr);

  need_update_current_frame_ = true;
  last_removed_timestamp_ = -1;
  error_occurred_ = false;
  stream_ended_ = false;
  loop_filter_manager_.Reset();
}

void VpxVideoDecoder::ReportError(const std::string& error_message) {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  SB_DLOG(ERROR) << error_message;
  error_occurred_ = true;
  error_cb_(kSbPlayerErrorDecode, error_message);
}

void VpxVideoDecoder::InitializeCodec() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());
  SB_DCHECK(!context_);
  SB_DCHECK(!texture_thread_);

  texture_thread_.reset(new JobThread("vpx_texture_thread"));
  texture_thread_->job_queue()->Schedule(
      std::bind(&VpxVideoDecoder::InitializeTextureThread, this));

  context_.reset(new vpx_codec_ctx);
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = current_frame_width_;
  vpx_config.h = current_frame_height_;
  vpx_config.threads = 4;

  vpx_codec_err_t status = vpx_codec_dec_init(
      context_.get(), &vpx_codec_vp9_dx_algo, &vpx_config,
      VPX_CODEC_USE_FRAME_THREADING | VPX_CODEC_USE_METAL_LF);
  if (status != VPX_CODEC_OK) {
    ReportError(
        FormatString("vpx_codec_dec_init() failed with status %d.", status));
    context_.reset();
  }

  // TODO: Enable loop filter optimization if we ever enable HDR here.
  // status = vpx_codec_control(context_.get(), VP9D_SET_LOOP_FILTER_OPT, 1);
  // SB_CHECK(status == VPX_CODEC_OK);
}

void VpxVideoDecoder::TeardownCodec(bool clear_all_decoded_frames) {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  if (texture_thread_) {
    // Wait until |texture_thread_| generated all textures.
    texture_thread_->job_queue()->Schedule(
        std::bind(&VpxVideoDecoder::TeardownTextureThread, this,
                  clear_all_decoded_frames));
    texture_thread_.reset();
  }
  SB_DCHECK(decoded_images_.empty());
  SB_DCHECK(decoded_images_size_.load() == 0);

  if (context_) {
    vpx_codec_destroy(context_.get());
    context_.reset();
  }
  total_input_frames_ = 0;
  total_output_frames_ = 0;
}

void VpxVideoDecoder::DecodeOneBuffer() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  if (decoded_images_size_.load() >= kMaxSizeOfDecodedImages ||
      decode_target_data_queue_size_.load() >= kMaxNumberOfCachedFrames) {
    decoder_thread_->job_queue()->Schedule(
        std::bind(&VpxVideoDecoder::DecodeOneBuffer, this), 5000);
    return;
  }

  scoped_refptr<InputBuffer> input_buffer;
  size_t pending_buffers_total_size_in_bytes = 0;
  size_t pending_buffers_count = 0;
  {
    ScopedLock lock(pending_buffers_mutex_);
    if (pending_buffers_.empty()) {
      decoder_thread_->job_queue()->Schedule(
          std::bind(&VpxVideoDecoder::DecodeOneBuffer, this), 5000);
      return;
    }
    input_buffer = pending_buffers_.front();
    pending_buffers_.pop_front();
    pending_buffers_total_size_in_bytes_ -= input_buffer->size();
    pending_buffers_total_size_in_bytes = pending_buffers_total_size_in_bytes_;
    pending_buffers_count = pending_buffers_.size();
  }

  decoder_thread_->job_queue()->Schedule(
      std::bind(&VpxVideoDecoder::DecodeOneBuffer, this));

  const auto& stream_info = input_buffer->video_stream_info();
  if (!context_ || stream_info.frame_width != current_frame_width_ ||
      stream_info.frame_height != current_frame_height_) {
    if (context_) {
      FlushPendingBuffers();
      TeardownCodec(false);
    }
    current_frame_width_ = stream_info.frame_width;
    current_frame_height_ = stream_info.frame_height;
    InitializeCodec();
  }

  SB_DCHECK(context_);

  loop_filter_manager_.Update(input_buffer, decode_target_data_queue_size_,
                              pending_buffers_total_size_in_bytes,
                              pending_buffers_count);

  bool loop_filter_disabled = loop_filter_manager_.is_loop_filter_disabled();
  if (loop_filter_disabled != loop_filter_disabled_) {
    loop_filter_disabled_ = loop_filter_disabled;
    vpx_codec_control_(context_.get(), VP9_SET_SKIP_LOOP_FILTER,
                       loop_filter_disabled_);
    if (loop_filter_disabled_) {
      SB_LOG(INFO) << "Start to skip loop filter for frame "
                   << input_buffer->timestamp()
                   << " total frames with loop filter skipped: "
                   << frames_with_loop_filter_disabled_
                   << " with frame backlog " << decode_target_data_queue_size_;
    } else {
      SB_LOG(INFO) << "Stopped skipping loop filter for frame "
                   << input_buffer->timestamp()
                   << " total frames with loop filter skipped: "
                   << frames_with_loop_filter_disabled_
                   << " with frame backlog " << decode_target_data_queue_size_;
    }
  }
  if (loop_filter_disabled_) {
    ++frames_with_loop_filter_disabled_;
  }

  int64_t timestamp = input_buffer->timestamp();
  static_assert(sizeof(timestamp) == sizeof(void*),
                "sizeof(timestamp) must equal to sizeof(void*).");
  vpx_codec_err_t status = vpx_codec_decode(
      context_.get(), input_buffer->data(), input_buffer->size(),
      reinterpret_cast<void*>(timestamp), 0);
  total_input_frames_++;
  if (status != VPX_CODEC_OK) {
    ReportError(
        FormatString("vpx_codec_decode() failed with status %d.", status));
    return;
  }
  TryGetOneFrame();
}

void VpxVideoDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());
  {
    ScopedLock lock(pending_buffers_mutex_);
    if (!pending_buffers_.empty()) {
      decoder_thread_->job_queue()->Schedule(
          std::bind(&VpxVideoDecoder::DecodeEndOfStream, this), 32000);
      return;
    }
  }
  SB_DCHECK(pending_buffers_total_size_in_bytes_ == 0);
  FlushPendingBuffers();
  texture_thread_->job_queue()->Schedule(
      std::bind(&VpxVideoDecoder::GenerateEndOfStream, this));
}

void VpxVideoDecoder::FlushPendingBuffers() {
  // Flush frames in decoder.
  while (total_output_frames_ < total_input_frames_) {
    // Pass |kSbInt64Max| as timestamp for the end of stream frame. It shouldn't
    // be used but just in case.
    const int64_t kEndOfStreamTimestamp = kSbInt64Max;
    vpx_codec_err_t status =
        vpx_codec_decode(context_.get(), 0, 0,
                         reinterpret_cast<void*>(kEndOfStreamTimestamp), 0);
    if (status != VPX_CODEC_OK) {
      ReportError(
          FormatString("vpx_codec_decode() failed with status %d.", status));
      return;
    }
    if (!TryGetOneFrame()) {
      ReportError("Failed to flush frames in decoder.");
      return;
    }
  }
}

bool VpxVideoDecoder::TryGetOneFrame() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  vpx_codec_iter_t dummy = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(context_.get(), &dummy);
  if (!vpx_image) {
    // No output yet.
    return false;
  }

  total_output_frames_++;
  if (vpx_image->fmt != VPX_IMG_FMT_I420) {
    ReportError(FormatString("Invalid vpx_image->fmt: %d.", vpx_image->fmt));
    SB_NOTIMPLEMENTED();
    return false;
  }

  SB_DCHECK(vpx_image->stride[VPX_PLANE_Y] ==
            vpx_image->stride[VPX_PLANE_U] * 2);
  SB_DCHECK(vpx_image->stride[VPX_PLANE_U] == vpx_image->stride[VPX_PLANE_V]);
  SB_DCHECK(vpx_image->planes[VPX_PLANE_Y] < vpx_image->planes[VPX_PLANE_U]);
  SB_DCHECK(vpx_image->planes[VPX_PLANE_U] < vpx_image->planes[VPX_PLANE_V]);

  if (vpx_image->stride[VPX_PLANE_Y] != vpx_image->stride[VPX_PLANE_U] * 2 ||
      vpx_image->stride[VPX_PLANE_U] != vpx_image->stride[VPX_PLANE_V] ||
      vpx_image->planes[VPX_PLANE_Y] >= vpx_image->planes[VPX_PLANE_U] ||
      vpx_image->planes[VPX_PLANE_U] >= vpx_image->planes[VPX_PLANE_V]) {
    ReportError("Invalid yuv plane format.");
    return false;
  }

  SB_DCHECK(vpx_image->d_w == current_frame_width_);
  SB_DCHECK(vpx_image->d_h == current_frame_height_);
  if (vpx_image->d_w != current_frame_width_ ||
      vpx_image->d_h != current_frame_height_) {
    ReportError("Invalid frame size.");
    return false;
  }

  int64_t timestamp = reinterpret_cast<int64_t>(vpx_image->user_priv);

  std::unique_ptr<DecodedImageData> decoded_image(
      new DecodedImageData(timestamp, vpx_image));
  {
    ScopedLock lock(decoded_images_mutex_);
    decoded_images_.push(std::move(decoded_image));
    decoded_images_size_ = decoded_images_.size();
  }
  texture_thread_->job_queue()->Schedule(
      std::bind(&VpxVideoDecoder::GenerateDecodeTargetData, this));
  return true;
}

void VpxVideoDecoder::InitializeTextureThread() {
  SB_DCHECK(texture_thread_->job_queue()->BelongsToCurrentThread());
  SB_DCHECK(!texture_provider_);

  texture_provider_ =
      new TextureProvider(decode_target_graphics_context_provider_);

  @autoreleasepool {
    SBDEglAdapter* egl_adapter = SBDGetApplication().eglAdapter;
    EAGLContext* egl_context =
        [egl_adapter applicationContextForStarboardContext:
                         decode_target_graphics_context_provider_->egl_context];

    egl_context_ = [egl_adapter contextWithSharedContext:egl_context];
    [egl_adapter setCurrentContext:egl_context_ andBindToSurface:nil];
  }  // @autoreleasepool
}

void VpxVideoDecoder::TeardownTextureThread(bool clear_all_decoded_frames) {
  SB_DCHECK(texture_thread_->job_queue()->BelongsToCurrentThread());
  SB_DCHECK(texture_provider_);

  if (clear_all_decoded_frames) {
    ScopedLock lock(decode_target_data_queue_mutex_);
    decode_target_data_queue_ = std::queue<scoped_refptr<DecodeTargetData>>();
    decode_target_data_queue_size_ = 0;
  }
  texture_provider_ = nullptr;

  @autoreleasepool {
    SBDEglAdapter* egl_adapter = SBDGetApplication().eglAdapter;
    [egl_adapter destroyContext:egl_context_];
    egl_context_ = nullptr;
  }  // @autoreleasepool
}

void VpxVideoDecoder::GenerateDecodeTargetData() {
  SB_DCHECK(texture_thread_->job_queue()->BelongsToCurrentThread());

  std::unique_ptr<DecodedImageData> decoded_image;
  {
    ScopedLock lock(decoded_images_mutex_);
    SB_DCHECK(!decoded_images_.empty());
    decoded_image = std::move(decoded_images_.front());
    decoded_images_.pop();
    decoded_images_size_ = decoded_images_.size();
  }

  scoped_refptr<DecodeTargetData> decode_target_data(
      new DecodeTargetData(texture_provider_, *decoded_image));

  scoped_refptr<VideoFrame> video_frame(new VideoFrameImpl(
      decoded_image->timestamp(),
      std::bind(&VpxVideoDecoder::RemoveDecodedImageByTimestamp, this,
                decoded_image->timestamp())));

  // Release holding output buffer as soon as possible.
  decoded_image.reset();

  {
    ScopedLock lock(decode_target_data_queue_mutex_);
    decode_target_data_queue_.push(decode_target_data);
    ++decode_target_data_queue_size_;
  }
  decoder_status_cb_(kNeedMoreInput, video_frame);
}

void VpxVideoDecoder::GenerateEndOfStream() {
  SB_DCHECK(texture_thread_->job_queue()->BelongsToCurrentThread());
  // |decoded_images_| should not be accessed in other threads at
  // this point.
  SB_DCHECK(decoded_images_.empty());

  decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
}

void VpxVideoDecoder::RemoveDecodedImageByTimestamp(int64_t timestamp) {
  last_removed_timestamp_ = timestamp;
  need_update_current_frame_ = true;
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VpxVideoDecoder::GetCurrentDecodeTarget() {
  ScopedLock lock(decode_target_data_queue_mutex_);
  int64_t last_removed_timestamp = last_removed_timestamp_.load();
  while (!decode_target_data_queue_.empty() &&
         decode_target_data_queue_.front()->timestamp() <=
             last_removed_timestamp) {
    decode_target_data_queue_.pop();
    --decode_target_data_queue_size_;
  }
  if (!current_decode_target_data_ && decode_target_data_queue_.empty()) {
    return kSbDecodeTargetInvalid;
  }
  if (!decode_target_data_queue_.empty() && need_update_current_frame_.load()) {
    need_update_current_frame_ = false;
    current_decode_target_data_ = decode_target_data_queue_.front();
  }
  return new SbDecodeTargetPrivate(current_decode_target_data_,
                                   current_decode_target_data_->timestamp());
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
