// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_PICTURE_H_
#define MEDIA_VIDEO_PICTURE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

// TODO(vmr): Evaluate the generalization potential of these interfaces &
//            classes and refactor as needed with the rest of media stack.
class PictureBuffer : public VideoDecodeAccelerator::PictureBuffer {
 public:
  PictureBuffer(int32 id, gfx::Size pixel_size,
                std::vector<uint32> color_format, MemoryType memory_type,
                std::vector<DataPlaneHandle> data_plane_handles);
  virtual ~PictureBuffer();

  // VideoDecodeAccelerator::PictureBuffer implementation.
  virtual int32 GetId() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual const std::vector<uint32>& GetColorFormat() OVERRIDE;
  virtual MemoryType GetMemoryType() OVERRIDE;
  virtual std::vector<DataPlaneHandle>& GetPlaneHandles() OVERRIDE;

 private:
  int32 id_;
  gfx::Size pixel_size_;
  std::vector<uint32> color_format_;
  MemoryType memory_type_;
  std::vector<DataPlaneHandle>& data_plane_handles_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PictureBuffer);
};

class Picture : public VideoDecodeAccelerator::Picture {
 public:
  Picture(PictureBuffer* picture_buffer, gfx::Size decoded_pixel_size,
          gfx::Size visible_pixel_size, void* user_handle);
  virtual ~Picture();

  // VideoDecodeAccelerator::Picture implementation.
  virtual PictureBuffer* picture_buffer() OVERRIDE;
  virtual gfx::Size GetDecodedSize() const OVERRIDE;
  virtual gfx::Size GetVisibleSize() const OVERRIDE;
  virtual void* GetUserHandle() OVERRIDE;

 private:
  // Pointer to the picture buffer which contains this picture.
  PictureBuffer* picture_buffer_;
  gfx::Size decoded_pixel_size_;
  gfx::Size visible_pixel_size_;
  void* user_handle_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Picture);
};

}  // namespace media

#endif  // MEDIA_VIDEO_PICTURE_H_
