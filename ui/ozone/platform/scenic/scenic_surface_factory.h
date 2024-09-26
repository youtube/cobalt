// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/session.h>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/handle.h"
#include "ui/ozone/platform/scenic/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_manager.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class ScenicSurface;

class ScenicSurfaceFactory : public SurfaceFactoryOzone {
 public:
  ScenicSurfaceFactory();

  ScenicSurfaceFactory(const ScenicSurfaceFactory&) = delete;
  ScenicSurfaceFactory& operator=(const ScenicSurfaceFactory&) = delete;

  ~ScenicSurfaceFactory() override;

  // Initializes the surface factory. Binds the surface factory to the
  // current thread (and thus must run with a message loop).
  void Initialize(mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host);

  // Disconnects from ScenicGpuHost and detaches from the current thread.
  // After shutting down, it is safe to call Initialize() again.
  void Shutdown();

  // SurfaceFactoryOzone implementation.
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation) override;
  std::unique_ptr<PlatformWindowSurface> CreatePlatformWindowSurface(
      gfx::AcceleratedWidget widget) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      absl::optional<gfx::Size> framebuffer_size = absl::nullopt) override;
  void CreateNativePixmapAsync(gfx::AcceleratedWidget widget,
                               gpu::VulkanDeviceQueue* device_queue,
                               gfx::Size size,
                               gfx::BufferFormat format,
                               gfx::BufferUsage usage,
                               NativePixmapCallback callback) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle) override;
  std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation(
      bool use_swiftshader,
      bool allow_protected_memory) override;
  std::vector<gfx::BufferFormat> GetSupportedFormatsForTexturing()
      const override;

  // Registers a surface for a |widget|.
  //
  // Must be called on the thread that owns the surface.
  void AddSurface(gfx::AcceleratedWidget widget, ScenicSurface* surface)
      LOCKS_EXCLUDED(surface_lock_);

  // Removes a surface for a |widget|.
  //
  // Must be called on the thread that owns the surface.
  void RemoveSurface(gfx::AcceleratedWidget widget)
      LOCKS_EXCLUDED(surface_lock_);

  // Returns the surface for a |widget|.
  //
  // Must be called on the thread that owns the surface.
  ScenicSurface* GetSurface(gfx::AcceleratedWidget widget)
      LOCKS_EXCLUDED(surface_lock_);

  // Creates a new scenic session on any thread.
  scenic::SessionPtrAndListenerRequest CreateScenicSession();

 private:
  // Links a surface to its parent window in the host process.
  void AttachSurfaceToWindow(
      gfx::AcceleratedWidget window,
      mojo::PlatformHandle surface_view_holder_token_mojo);

  base::flat_map<gfx::AcceleratedWidget, ScenicSurface*> surface_map_
      GUARDED_BY(surface_lock_);
  base::Lock surface_lock_;

  mojo::Remote<mojom::ScenicGpuHost> gpu_host_;
  std::unique_ptr<GLOzone> egl_implementation_;

  fuchsia::ui::scenic::ScenicPtr scenic_;

  // Task runner for thread that |scenic_| and |gpu_host_| are bound on.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  SysmemBufferManager sysmem_buffer_manager_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ScenicSurfaceFactory> weak_ptr_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_
