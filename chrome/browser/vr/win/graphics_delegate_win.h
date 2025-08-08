// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_
#define CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class SharedImageInterface;
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace vr {

class GraphicsDelegateWin : public GraphicsDelegate {
 public:
  GraphicsDelegateWin();
  ~GraphicsDelegateWin() override;

  // GraphicsDelegate:
  bool PreRender() override;
  void PostRender() override;
  mojo::PlatformHandle GetTexture() override;
  const gpu::SyncToken& GetSyncToken() override;
  void ResetMemoryBuffer() override;
  bool BindContext() override;
  void ClearContext() override;

 private:
  // Helpers:
  bool EnsureMemoryBuffer();
  void ClearBufferToBlack() override;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_ = nullptr;
  raw_ptr<gpu::SharedImageInterface> sii_ = nullptr;
  gfx::Size last_size_;
  gpu::Mailbox mailbox_;  // Corresponding to our target GpuMemoryBuffer.
  GLuint dest_texture_id_ = 0;
  GLuint draw_frame_buffer_ = 0;
  gfx::GpuMemoryBufferHandle buffer_handle_;
  // Sync point after access to the buffer is done.
  gpu::SyncToken access_done_sync_token_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_
