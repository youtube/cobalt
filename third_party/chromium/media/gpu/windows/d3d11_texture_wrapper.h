// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/status.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

using CommandBufferHelperPtr = scoped_refptr<CommandBufferHelper>;
using MailboxHolderArray = gpu::MailboxHolder[VideoFrame::kMaxPlanes];
using GetCommandBufferHelperCB =
    base::RepeatingCallback<CommandBufferHelperPtr()>;

// Support different strategies for processing pictures - some may need copying,
// for example.  Each wrapper owns the resources for a single texture, so it's
// up to you not to re-use a wrapper for a second image before a previously
// processed image is no longer needed.
class MEDIA_GPU_EXPORT Texture2DWrapper {
 public:
  Texture2DWrapper();
  virtual ~Texture2DWrapper();

  // Initialize the wrapper.
  virtual Status Init(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferHelperCB get_helper_cb,
      ComD3D11Texture2D texture,
      size_t array_size) = 0;

  // If the |texture| has key mutex, it is important to acquire the key mutex
  // before any usage or you'll get an error. This API is required to be called:
  // - Before reading or writing to the texture via views on the texture or
  // other means.
  // - Before calling ProcessTexture.
  // And need to call ProcessTexture() to release the key mutex.
  virtual Status AcquireKeyedMutexIfNeeded() = 0;

  // Import |texture|, |array_slice| and return the mailbox(es) that can be
  // used to refer to it.
  virtual Status ProcessTexture(const gfx::ColorSpace& input_color_space,
                                MailboxHolderArray* mailbox_dest_out,
                                gfx::ColorSpace* output_color_space) = 0;

  virtual void SetStreamHDRMetadata(
      const gfx::HDRMetadata& stream_metadata) = 0;
  virtual void SetDisplayHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata) = 0;
};

// The default texture wrapper that uses GPUResources to talk to hardware
// on behalf of a Texture2D.  Each DefaultTexture2DWrapper owns GL textures
// that it uses to bind the provided input texture.  Thus, one needs one wrapper
// instance for each concurrently outstanding texture.
class MEDIA_GPU_EXPORT DefaultTexture2DWrapper : public Texture2DWrapper {
 public:
  // Error callback for GpuResource to notify us of errors.
  using OnErrorCB = base::OnceCallback<void(Status)>;

  // While the specific texture instance can change on every call to
  // ProcessTexture, the dxgi format must be the same for all of them.
  DefaultTexture2DWrapper(const gfx::Size& size, DXGI_FORMAT dxgi_format);
  ~DefaultTexture2DWrapper() override;

  Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
              GetCommandBufferHelperCB get_helper_cb,
              ComD3D11Texture2D in_texture,
              size_t array_slice) override;

  Status AcquireKeyedMutexIfNeeded() override;

  Status ProcessTexture(const gfx::ColorSpace& input_color_space,
                        MailboxHolderArray* mailbox_dest,
                        gfx::ColorSpace* output_color_space) override;

  void SetStreamHDRMetadata(const gfx::HDRMetadata& stream_metadata) override;
  void SetDisplayHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata) override;

 private:
  // Things that are to be accessed / freed only on the main thread.  In
  // addition to setting up the textures to render from a D3D11 texture,
  // these also hold the chrome GL Texture objects so that the client
  // can use the mailbox.
  class GpuResources {
   public:
    GpuResources(OnErrorCB on_error_cb,
                 GetCommandBufferHelperCB get_helper_cb,
                 const std::vector<gpu::Mailbox>& mailboxes,
                 const gfx::Size& size,
                 DXGI_FORMAT dxgi_format,
                 ComD3D11Texture2D texture,
                 size_t array_slice);

    GpuResources(const GpuResources&) = delete;
    GpuResources& operator=(const GpuResources&) = delete;

    ~GpuResources();

   private:
    scoped_refptr<CommandBufferHelper> helper_;

    std::vector<std::unique_ptr<gpu::SharedImageRepresentationFactoryRef>>
        shared_images_;
  };

  // Receive an error from |gpu_resources_| and store it in |received_error_|.
  void OnError(Status status);

  // The first error status that we've received from |gpu_resources_|, if any.
  absl::optional<Status> received_error_;

  gfx::Size size_;
  base::SequenceBound<GpuResources> gpu_resources_;
  MailboxHolderArray mailbox_holders_;
  DXGI_FORMAT dxgi_format_;

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  bool keyed_mutex_acquired_ = false;

  base::WeakPtrFactory<DefaultTexture2DWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_
