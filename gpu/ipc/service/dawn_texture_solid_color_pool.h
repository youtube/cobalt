// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DAWN_TEXTURE_SOLID_COLOR_POOL_H_
#define GPU_IPC_SERVICE_DAWN_TEXTURE_SOLID_COLOR_POOL_H_

#include <windows.h>

#include <d3d12.h>
#include <dcomp.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

// clang-format off
#include <webgpu/webgpu_cpp.h>
// clang-format on

#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/dc_commit_error.h"
#include "ui/gl/solid_color_pool_base.h"

namespace gpu {

class MockDawnTextureSolidColorPool;

// `SolidColorPoolBase` implementation that produces `IDCompositionTexture`
// entries wrapping 1x1 `ID3D12Resource`s rendered to via Dawn.
class GPU_IPC_SERVICE_EXPORT DawnTextureSolidColorPool
    : public gl::SolidColorPoolBase {
 public:
  DawnTextureSolidColorPool(
      wgpu::Device device,
      Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue,
      Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device);
  ~DawnTextureSolidColorPool() override;

  DawnTextureSolidColorPool(const DawnTextureSolidColorPool&) = delete;
  DawnTextureSolidColorPool& operator=(const DawnTextureSolidColorPool&) =
      delete;

  class GPU_IPC_SERVICE_EXPORT TextureEntry final : public Entry {
   public:
    // Allocates a `ID3D12Resource`, wraps it in an `IDCompositionTexture`,
    // and imports that into Dawn as a `wgpu::SharedTextureMemory`.
    static base::expected<std::unique_ptr<TextureEntry>, gl::CommitError>
    Create(ID3D12Device* d3d12_device,
           IDCompositionDevice4* dcomp_device4,
           const wgpu::Device& device);

    ~TextureEntry() override;

    IUnknown* GetContent() const override;

    ID3D12Resource* d3d12_resource() const { return d3d12_resource_.Get(); }
    IDCompositionTexture* dcomp_texture() const { return dcomp_texture_.Get(); }
    const wgpu::SharedTextureMemory& shared_texture_memory() const {
      return shared_texture_memory_;
    }

   private:
    friend class MockDawnTextureSolidColorPool;

    TextureEntry(Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource,
                 Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture,
                 wgpu::SharedTextureMemory shared_texture_memory);

    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource_;
    Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture_;
    wgpu::SharedTextureMemory shared_texture_memory_;
  };

  // gl::SolidColorPoolBase methods.
  base::expected<void, gl::CommitError> FlushPendingFillsBeforeCommit()
      override;

 protected:
  DawnTextureSolidColorPool();

  // gl::SolidColorPoolBase:
  base::expected<void, gl::CommitError> FillEntry(
      std::unique_ptr<Entry>& entry,
      const SkColor4f& color) override;

  // Builds a fresh entry from the pool's devices. Can be overridden in tests.
  virtual base::expected<std::unique_ptr<TextureEntry>, gl::CommitError>
  CreateEntry();

  // Result of polling the existing `IDCompositionTexture` for its
  // availability fence.
  struct FenceCheckResult {
    // Set to true DComp may still be scanning out the previous texture (the API
    // returned a null fence); the caller must drop the entry's D3D12/DComp/Dawn
    // members and rebuild
    bool needs_rebuild = false;
    // when non-null, is the fence the GPU must wait on before writing to the
    // texture; when null, the previous fill is complete and no wait is
    // necessary.
    scoped_refptr<gfx::D3DSharedFence> wait_fence;
  };
  virtual FenceCheckResult CheckAvailableFence(
      IDCompositionTexture* dcomp_texture);

  // Fills `shared_texture_memory` with the solid `color`. `wait_fence`, is a
  // fence the GPU must wait on before writing; pass null when no wait is
  // needed. Set `already_initialized=true` when the texture already holds valid
  // contents, letting Dawn skip its lazy clear.
  virtual base::expected<void, gl::CommitError> EncodeFill(
      const wgpu::SharedTextureMemory& shared_texture_memory,
      const SkColor4f& color,
      scoped_refptr<gfx::D3DSharedFence> wait_fence,
      bool already_initialized);

 private:
  // Lazily opens `encoder_`.
  wgpu::CommandEncoder EnsureEncoder();

  wgpu::Device device_;
  wgpu::Queue queue_;
  Microsoft::WRL::ComPtr<IDCompositionDevice4> dcomp_device4_;
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;

  // Per-frame encoder. Lazily opened by `EncodeFillAndBeginAccess`,
  // finalized and submitted in `FlushPendingFillsBeforeCommit`.
  wgpu::CommandEncoder encoder_;
};

GPU_IPC_SERVICE_EXPORT gl::SolidColorPoolFactory
CreateDawnTextureSolidColorPoolFactory(
    wgpu::Device device,
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue);

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DAWN_TEXTURE_SOLID_COLOR_POOL_H_
