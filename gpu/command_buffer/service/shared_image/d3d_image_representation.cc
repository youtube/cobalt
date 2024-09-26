// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_representation.h"

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

GLTexturePassthroughD3DImageRepresentation::
    GLTexturePassthroughD3DImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        std::vector<scoped_refptr<D3DImageBacking::GLTextureHolder>>
            gl_texture_holders)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      gl_texture_holders_(std::move(gl_texture_holders)) {}

GLTexturePassthroughD3DImageRepresentation::
    ~GLTexturePassthroughD3DImageRepresentation() = default;

bool GLTexturePassthroughD3DImageRepresentation::
    NeedsSuspendAccessForDXGIKeyedMutex() const {
  return static_cast<D3DImageBacking*>(backing())->has_keyed_mutex();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughD3DImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  return gl_texture_holders_[plane_index]->texture_passthrough();
}

void* GLTexturePassthroughD3DImageRepresentation::GetEGLImage() {
  DCHECK(format().is_single_plane());
  return gl_texture_holders_[0]->egl_image();
}

bool GLTexturePassthroughD3DImageRepresentation::BeginAccess(GLenum mode) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
    // Clear any textures that are marked as needing binding (note that there is
    // no actual "binding" action that is necessary to take here, as none of the
    // GLImages that can be attached to a texture when it is marked as binding
    // actually need to be bound lazily to the texture).
    GetTexturePassthrough(plane)->clear_bind_pending();
  }
  bool write_access = mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  return d3d_image_backing->BeginAccessD3D11(write_access);
}

void GLTexturePassthroughD3DImageRepresentation::EndAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

#if BUILDFLAG(USE_DAWN)
DawnD3DImageRepresentation::DawnD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

DawnD3DImageRepresentation::~DawnD3DImageRepresentation() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture DawnD3DImageRepresentation::BeginAccess(WGPUTextureUsage usage) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  texture_ = d3d_image_backing->BeginAccessDawn(device_, usage);
  return texture_;
}

void DawnD3DImageRepresentation::EndAccess() {
  if (!texture_)
    return;

  // Do this before further operations since those could end up destroying the
  // Dawn device and we want the fence to be duplicated before then.
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessDawn(device_, texture_);

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}
#endif  // BUILDFLAG(USE_DAWN)

OverlayD3DImageRepresentation::OverlayD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : OverlayImageRepresentation(manager, backing, tracker) {}

OverlayD3DImageRepresentation::~OverlayD3DImageRepresentation() = default;

bool OverlayD3DImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  return static_cast<D3DImageBacking*>(backing())->BeginAccessD3D11(
      /*write_access=*/false);
}

void OverlayD3DImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  static_cast<D3DImageBacking*>(backing())->EndAccessD3D11();
}

absl::optional<gl::DCLayerOverlayImage>
OverlayD3DImageRepresentation::GetDCLayerOverlayImage() {
  return static_cast<D3DImageBacking*>(backing())->GetDCLayerOverlayImage();
}

D3D11VideoDecodeImageRepresentation::D3D11VideoDecodeImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    : VideoDecodeImageRepresentation(manager, backing, tracker),
      texture_(std::move(texture)) {}

D3D11VideoDecodeImageRepresentation::~D3D11VideoDecodeImageRepresentation() =
    default;

bool D3D11VideoDecodeImageRepresentation::BeginWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  if (!d3d_image_backing->BeginAccessD3D11(/*write_access=*/true))
    return false;

  return true;
}

void D3D11VideoDecodeImageRepresentation::EndWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
D3D11VideoDecodeImageRepresentation::GetD3D11Texture() const {
  return texture_;
}

}  // namespace gpu
