// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_
#define MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/media_log.h"
#include "media/base/status.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_image.h"

namespace media {

class Texture2DWrapper;

// PictureBuffer that owns Chrome Textures to display it, and keep a reference
// to the D3D texture that backs the image.
//
// This is created and owned on the decoder thread.  While currently that's the
// gpu main thread, we still keep the decoder parts separate from the chrome GL
// parts, in case that changes.
//
// This is refcounted so that VideoFrame can hold onto it indirectly.  While
// the chrome Texture is sufficient to keep the pictures renderable, we still
// need to guarantee that the client has time to use the mailbox.  Once it
// does so, it would be fine if this were destroyed.  Technically, only the
// GpuResources have to be retained until the mailbox is used, but we just
// retain the whole thing.
class MEDIA_GPU_EXPORT D3D11PictureBuffer
    : public base::RefCountedDeleteOnSequence<D3D11PictureBuffer> {
 public:
  // |texture_wrapper| is responsible for controlling mailbox access to
  // the ID3D11Texture2D,
  // |array_slice| is the picturebuffer index inside the Array-type
  // ID3D11Texture2D.  |picture_index| is a unique id used to identify this
  // picture to the decoder.  If a texture array is used, then it might as well
  // be equal to the texture array index.  Otherwise, any 0-based index is
  // probably okay, though sequential makes sense.
  D3D11PictureBuffer(
      scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
      ComD3D11Texture2D texture,
      size_t array_slice,
      std::unique_ptr<Texture2DWrapper> texture_wrapper,
      gfx::Size size,
      size_t picture_index);

  Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
              GetCommandBufferHelperCB get_helper_cb,
              ComD3D11VideoDevice video_device,
              const GUID& decoder_guid,
              std::unique_ptr<MediaLog> media_log);

  // Set the contents of a mailbox holder array, return true if successful.
  // |input_color_space| is the color space of our input texture, and
  // |output_color_space| will be set, on success, to the color space that the
  // processed texture has.
  Status ProcessTexture(const gfx::ColorSpace& input_color_space,
                        MailboxHolderArray* mailbox_dest,
                        gfx::ColorSpace* output_color_space);
  ComD3D11Texture2D Texture() const;
  StatusOr<ID3D11VideoDecoderOutputView*> AcquireOutputView() const;

  const gfx::Size& size() const { return size_; }
  size_t picture_index() const { return picture_index_; }

  // Is this PictureBuffer backing a VideoFrame right now?
  bool in_client_use() const { return in_client_use_ > 0; }

  // Is this PictureBuffer holding an image that's in use by the decoder?
  bool in_picture_use() const { return in_picture_use_; }

  void add_client_use() {
    in_client_use_++;
    DCHECK_GT(in_client_use_, 0);
  }
  void remove_client_use() {
    DCHECK_GT(in_client_use_, 0);
    in_client_use_--;
  }
  void set_in_picture_use(bool use) { in_picture_use_ = use; }

  Texture2DWrapper* texture_wrapper() const { return texture_wrapper_.get(); }

  // Shouldn't be here, but simpler for now.
  base::TimeDelta timestamp_;

 private:
  ~D3D11PictureBuffer();
  friend class base::RefCountedDeleteOnSequence<D3D11PictureBuffer>;
  friend class base::DeleteHelper<D3D11PictureBuffer>;

  ComD3D11Texture2D texture_;
  uint32_t array_slice_;

  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<Texture2DWrapper> texture_wrapper_;
  gfx::Size size_;
  bool in_picture_use_ = false;
  int in_client_use_ = 0;
  size_t picture_index_;

  ComD3D11VideoDecoderOutputView output_view_;

  DISALLOW_COPY_AND_ASSIGN(D3D11PictureBuffer);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_
