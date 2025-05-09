// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/gpu/starboard/starboard_gpu_factory.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

#ifndef GL_ANGLE_texture_storage_external
#define GL_ANGLE_texture_storage_external 1
#define GL_TEXTURE_NATIVE_ID_ANGLE 0x3481
#endif /* GL_ANGLE_texture_storage_external */

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

}  // namespace

StarboardGpuFactory::StarboardGpuFactory(GetStubCB get_stub_cb) {
  DETACH_FROM_THREAD(thread_checker_);
  get_stub_cb_ = std::move(get_stub_cb);
}

StarboardGpuFactory::~StarboardGpuFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_) {
    stub_->RemoveDestructionObserver(this);
  }
}

void StarboardGpuFactory::Initialize(base::UnguessableToken channel_token,
                                     int32_t route_id,
                                     int texture_id,
                                     int* texture_native_id,
                                     base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stub_ = get_stub_cb_.Run(channel_token, route_id);
  if (stub_) {
    stub_->AddDestructionObserver(this);
  }

  // Copied from SurfaceTexture::Create()
  // ui/gl/android/surface_texture.cc
  int native_id = texture_id;
  // ANGLE emulates texture IDs so query the native ID of the texture.
  if (texture_id != 0 &&
      gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    GLint prev_texture = 0;
    auto* api = gl::g_current_gl_context;
    api->glGetIntegervFn(GL_TEXTURE_BINDING_EXTERNAL_OES, &prev_texture);
    api->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES, texture_id);
    api->glGetTexParameterivFn(GL_TEXTURE_EXTERNAL_OES,
                               GL_TEXTURE_NATIVE_ID_ANGLE, &native_id);
    api->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES, prev_texture);
  }
  // TODO(borongchen): pass |native_id| to DecodeTarget to create
  // SurfaceTexture.
  *texture_native_id = native_id;

  done_event->Signal();
}

void StarboardGpuFactory::RunDeleteTexturesOnGpu(
    int texture_native_id,
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    return;
  }

  if (texture_native_id) {
    GLuint texture_id = texture_native_id;
    auto* api = gl::g_current_gl_context;
    api->glDeleteTexturesFn(1, &texture_id);
  }

  done_event->Signal();
}

void StarboardGpuFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
}

}  // namespace media
