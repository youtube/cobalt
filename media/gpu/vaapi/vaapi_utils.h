// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_UTILS_H_
#define MEDIA_GPU_VAAPI_VAAPI_UTILS_H_

#include <va/va.h>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/thread_annotations.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class Lock;
}

namespace media {
class VaapiWrapper;
class Vp8ReferenceFrameVector;
struct VAContextAndScopedVASurfaceDeleter;
struct Vp8FrameHeader;

// Class to map a given VABuffer, identified by |buffer_id|, for its lifetime.
// This class must operate under |lock_| acquired.
class ScopedVABufferMapping {
 public:
  // |release_callback| will be called if the mapping of the buffer failed.
  ScopedVABufferMapping(const base::Lock* lock,
                        VADisplay va_display,
                        VABufferID buffer_id,
                        base::OnceCallback<void(VABufferID)> release_callback =
                            base::NullCallback());

  ScopedVABufferMapping(const ScopedVABufferMapping&) = delete;
  ScopedVABufferMapping& operator=(const ScopedVABufferMapping&) = delete;

  ~ScopedVABufferMapping();
  bool IsValid() const { return !!va_buffer_data_; }
  void* data() const {
    DCHECK(IsValid());
    return va_buffer_data_;
  }
  // Explicit destruction method, to retrieve the success/error result. It is
  // safe to call this method several times.
  VAStatus Unmap();

 private:
  const base::Lock* lock_;  // Only for AssertAcquired() calls.
  const VADisplay va_display_;
  const VABufferID buffer_id_;

  void* va_buffer_data_ = nullptr;
};

// This class tracks the VABuffer life cycle from vaCreateBuffer() to
// vaDestroyBuffer(). Users of this class are responsible for mapping and
// unmapping the buffer as needed. The destructor acquires |lock|, but the user
// of this class must acquire the lock prior to construction.
class ScopedVABuffer {
 public:
  // Creates ScopedVABuffer. Returns nullptr if creating the va buffer fails.
  static std::unique_ptr<ScopedVABuffer> Create(base::Lock* lock,
                                                VADisplay va_display,
                                                VAContextID va_context_id,
                                                VABufferType va_buffer_type,
                                                size_t size);
  static std::unique_ptr<ScopedVABuffer>
  CreateForTesting(VABufferID buffer_id, VABufferType buffer_type, size_t size);
  ScopedVABuffer(const ScopedVABuffer&) = delete;
  ScopedVABuffer& operator=(const ScopedVABuffer&) = delete;
  ~ScopedVABuffer();

  VABufferID id() const { return va_buffer_id_; }
  VABufferType type() const { return va_buffer_type_; }
  size_t size() const { return size_; }

 private:
  ScopedVABuffer(base::Lock* lock,
                 VADisplay va_display,
                 VABufferID va_buffer_id,
                 VABufferType va_buffer_type,
                 size_t size);

  base::Lock* const lock_;
  const VADisplay va_display_ GUARDED_BY(lock_);

  const VABufferID va_buffer_id_;
  const VABufferType va_buffer_type_;
  const size_t size_;
};

// This class tracks the VAImage life cycle from vaCreateImage() - vaGetImage()
// to vaDestroyImage(). In between creation and destruction, image()->buf  will
// try to be be mapped on user space using a ScopedVABufferMapping. All
// resources will be cleaned up appropriately. |lock| is acquired for
// destruction purposes.
class ScopedVAImage {
 public:
  ScopedVAImage(base::Lock* lock,
                VADisplay va_display,
                VASurfaceID va_surface_id,
                VAImageFormat* format /* Needs to be a pointer for libva */,
                const gfx::Size& size);

  ScopedVAImage(const ScopedVAImage&) = delete;
  ScopedVAImage& operator=(const ScopedVAImage&) = delete;

  ~ScopedVAImage();

  bool IsValid() const { return va_buffer_ && va_buffer_->IsValid(); }

  const VAImage* image() const { return image_.get(); }
  const ScopedVABufferMapping* va_buffer() const {
    DCHECK(IsValid());
    return va_buffer_.get();
  }

 private:
  base::Lock* lock_;
  const VADisplay va_display_ GUARDED_BY(lock_);
  std::unique_ptr<VAImage> image_;
  std::unique_ptr<ScopedVABufferMapping> va_buffer_;
};

// A VA-API-specific surface used by video/image codec accelerators to work on.
// As the name suggests, this class is self-cleaning.
class ScopedVASurface {
 public:
  ScopedVASurface(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                  VASurfaceID va_surface_id,
                  const gfx::Size& size,
                  unsigned int va_rt_format);

  ScopedVASurface(const ScopedVASurface&) = delete;
  ScopedVASurface& operator=(const ScopedVASurface&) = delete;

  ~ScopedVASurface();

  bool IsValid() const;
  VASurfaceID id() const { return va_surface_id_; }
  const gfx::Size& size() const { return size_; }
  unsigned int format() const { return va_rt_format_; }

 private:
  friend struct VAContextAndScopedVASurfaceDeleter;
  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  const VASurfaceID va_surface_id_;
  const gfx::Size size_;
  const unsigned int va_rt_format_;
};

// A combination of a numeric ID |id| and a callback to release it. This class
// makes no assumptions on threading or lifetimes; |release_cb_| must provide
// for this.
// ScopedID allows for object-specific release callbacks, whereas
// unique_ptr::deleter_type (or base::ScopedGeneric) only supports free
// functions (or class-static methods) for freeing.
template <typename T>
class ScopedID {
 public:
  using ReleaseCB = base::OnceCallback<void(T)>;

  ScopedID(T id, ReleaseCB release_cb)
      : id_(id), release_cb_(std::move(release_cb)) {
    DCHECK(release_cb_);
    static_assert(std::is_integral<T>::value, "T must be a numeric type.");
  }
  ~ScopedID() { std::move(release_cb_).Run(id_); }

  ScopedID& operator=(const ScopedID&) = delete;
  ScopedID(const ScopedID&) = delete;

  T id() const { return id_; }

 private:
  const T id_;
  ReleaseCB release_cb_;
};

// Adapts |frame_header| to the Vaapi data types.
void FillVP8DataStructures(const Vp8FrameHeader& frame_header,
                           const Vp8ReferenceFrameVector& reference_frames,
                           VAIQMatrixBufferVP8* iq_matrix_buf,
                           VAProbabilityDataBufferVP8* prob_buf,
                           VAPictureParameterBufferVP8* pic_param,
                           VASliceParameterBufferVP8* slice_param);

bool IsValidVABufferType(VABufferType type);

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_UTILS_H_
