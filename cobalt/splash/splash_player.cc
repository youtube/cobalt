// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/splash/splash_player.h"

#include "base/task/thread_pool.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"

namespace cobalt {
namespace splash {

SplashPlayer::SplashPlayer() {
  task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_VISIBLE});
}

SplashPlayer::~SplashPlayer() {
  Stop();
}

void SplashPlayer::Initialize() {
  CHECK(task_runner_->BelongsToCurrentThread());

  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);
  surface_ = gl::init::CreateOffscreenGLSurface(nullptr, gfx::Size());
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  context_->MakeCurrent(surface_.get());
}

void SplashPlayer::Play(const base::FilePath& video_path) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&SplashPlayer::Initialize,
                                                   base::Unretained(this)));
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&SplashPlayer::RenderFrame,
                                                   base::Unretained(this)));
}

void SplashPlayer::RenderFrame() {
  // TODO: Implement video decoding and rendering.
}

void SplashPlayer::Stop() {
  if (context_) {
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
  }
  surface_ = nullptr;
}

}  // namespace splash
}  // namespace cobalt
