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

#ifndef COBALT_SPLASH_SPLASH_PLAYER_H_
#define COBALT_SPLASH_SPLASH_PLAYER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libwebm/source/mkvparser/mkvparser.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace cobalt {
namespace splash {

class SplashPlayer {
 public:
  SplashPlayer();
  ~SplashPlayer();

  void Play(const base::FilePath& video_path);
  void Stop();

 private:
  void Initialize(const base::FilePath& video_path);
  void InitializeShaders();
  void DecodeFrame();
  void RenderFrame();

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  class BufferReader;
  std::vector<uint8_t> video_buffer_;
  BufferReader* reader_ = nullptr;
  mkvparser::Segment* segment_ = nullptr;
  const mkvparser::Cluster* cluster_ = nullptr;
  const mkvparser::Block* block_ = nullptr;
  const mkvparser::BlockEntry* block_entry_ = nullptr;
  int64_t block_frame_index_ = 0;

  vpx_codec_ctx_t vpx_codec_;
  vpx_codec_iter_t vpx_iter_ = nullptr;
  vpx_image_t* vpx_image_ = nullptr;

  GLuint program_ = 0;
  GLuint vertex_shader_ = 0;
  GLuint fragment_shader_ = 0;
  GLuint y_texture_ = 0;
  GLuint u_texture_ = 0;
  GLuint v_texture_ = 0;
};

}  // namespace splash
}  // namespace cobalt

#endif  // COBALT_SPLASH_SPLASH_PLAYER_H_
