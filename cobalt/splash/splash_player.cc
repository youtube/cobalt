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

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "ui/android/window_android.h"
#include "ui/gfx/frame_data.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace cobalt {
namespace splash {

namespace {

const char kVertexShader[] =
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = a_position;\n"
    "  v_texCoord = a_texCoord;\n"
    "}\n";

const char kFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D y_texture;\n"
    "uniform sampler2D u_texture;\n"
    "uniform sampler2D v_texture;\n"
    "void main() {\n"
    "  float y = texture2D(y_texture, v_texCoord).r;\n"
    "  float u = texture2D(u_texture, v_texCoord).r - 0.5;\n"
    "  float v = texture2D(v_texture, v_texCoord).r - 0.5;\n"
    "  float r = y + 1.402 * v;\n"
    "  float g = y - 0.344 * u - 0.714 * v;\n"
    "  float b = y + 1.772 * u;\n"
    "  gl_FragColor = vec4(r, g, b, 1.0);\n"
    "}\n";

GLuint CompileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 0) {
      std::unique_ptr<char[]> buf(new char[info_len]);
      if (buf) {
        glGetShaderInfoLog(shader, info_len, nullptr, buf.get());
        LOG(ERROR) << "Could not compile shader: " << buf.get();
      }
    }
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

class SplashPlayer::BufferReader : public mkvparser::IMkvReader {
 public:
  explicit BufferReader(const std::vector<uint8_t>& buffer) : buffer_(buffer) {}

  int Read(long long pos, long len, unsigned char* buf) override {
    if (pos < 0 || len < 0) {
      return -1;
    }
    if (static_cast<size_t>(pos) >= buffer_.size()) {
      return -1;
    }
    if (static_cast<size_t>(pos) + len > buffer_.size()) {
      len = buffer_.size() - pos;
    }
    memcpy(buf, buffer_.data() + pos, len);
    return 0;
  }

  int Length(long long* total, long long* available) override {
    *total = buffer_.size();
    *available = buffer_.size();
    return 0;
  }

 private:
  const std::vector<uint8_t>& buffer_;
};

SplashPlayer::SplashPlayer() {
  task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_VISIBLE});
}

SplashPlayer::~SplashPlayer() {
  Stop();
}

void SplashPlayer::Initialize(const base::FilePath& video_path,
                              gfx::AcceleratedWidget window) {
  CHECK(task_runner_->BelongsToCurrentThread());

  window_ = window;

  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);
  surface_ = gl::init::CreateViewGLSurface(gl::GetDefaultDisplayEGL(), window_);
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  context_->MakeCurrent(surface_.get());

  InitializeShaders();

  std::string video_data;
  if (!base::ReadFileToString(video_path, &video_data)) {
    LOG(ERROR) << "Failed to read video file.";
    return;
  }
  video_buffer_.assign(video_data.begin(), video_data.end());
  reader_ = new BufferReader(video_buffer_);

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(reader_, pos);

  mkvparser::Segment::CreateInstance(reader_, pos, segment_);
  segment_->Load();

  const mkvparser::Tracks* tracks = segment_->GetTracks();
  const mkvparser::Track* track = tracks->GetTrackByNumber(1);
  if (track == nullptr || track->GetType() != mkvparser::Track::kVideo) {
    LOG(ERROR) << "Could not find video track.";
    return;
  }

  vpx_codec_dec_cfg_t cfg = {0};
  vpx_codec_iface_t* iface = vpx_codec_vp9_dx();
  if (vpx_codec_dec_init(&vpx_codec_, iface, &cfg, 0)) {
    LOG(ERROR) << "Failed to initialize VP9 decoder.";
    return;
  }

  cluster_ = segment_->GetFirst();
}

void SplashPlayer::InitializeShaders() {
  vertex_shader_ = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  fragment_shader_ = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
  program_ = glCreateProgram();
  glAttachShader(program_, vertex_shader_);
  glAttachShader(program_, fragment_shader_);
  glLinkProgram(program_);
  glUseProgram(program_);

  glGenTextures(1, &y_texture_);
  glGenTextures(1, &u_texture_);
  glGenTextures(1, &v_texture_);
}

void SplashPlayer::Play(const base::FilePath& video_path,
                        gfx::AcceleratedWidget window) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SplashPlayer::Initialize,
                                base::Unretained(this), video_path, window));
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&SplashPlayer::DecodeFrame,
                                                   base::Unretained(this)));
}

void SplashPlayer::DecodeFrame() {
  if (!cluster_ || cluster_->EOS()) {
    completion_event_.Signal();
    return;
  }

  if (!block_entry_ ||
      block_frame_index_ >= block_entry_->GetBlock()->GetFrameCount()) {
    long status = cluster_->GetFirst(block_entry_);
    if (status < 0) {
      cluster_ = segment_->GetNext(cluster_);
      if (!cluster_ || cluster_->EOS()) {
        completion_event_.Signal();
        return;
      }
      cluster_->GetFirst(block_entry_);
    }
    block_frame_index_ = 0;
  }

  block_ = block_entry_->GetBlock();
  const mkvparser::Block::Frame& frame = block_->GetFrame(block_frame_index_);

  std::vector<uint8_t> frame_buffer(frame.len);
  frame.Read(reader_, &frame_buffer[0]);

  if (vpx_codec_decode(&vpx_codec_, frame_buffer.data(), frame_buffer.size(),
                       nullptr, 0)) {
    LOG(ERROR) << "Failed to decode frame.";
  } else {
    vpx_iter_ = nullptr;
    vpx_image_ = vpx_codec_get_frame(&vpx_codec_, &vpx_iter_);
    if (vpx_image_) {
      RenderFrame();
    }
  }

  block_frame_index_++;
  const mkvparser::BlockEntry* next_block_entry;
  if (cluster_->GetNext(block_entry_, next_block_entry) == 0) {
    block_entry_ = next_block_entry;
  }

  task_runner_->PostTask(FROM_HERE, base::BindOnce(&SplashPlayer::DecodeFrame,
                                                   base::Unretained(this)));
}

void SplashPlayer::RenderFrame() {
  CHECK(task_runner_->BelongsToCurrentThread());
  if (!context_->IsCurrent(surface_.get())) {
    context_->MakeCurrent(surface_.get());
  }

  glClear(GL_COLOR_BUFFER_BIT);

  if (vpx_image_) {
    glUseProgram(program_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, vpx_image_->d_w,
                 vpx_image_->d_h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 vpx_image_->planes[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(glGetUniformLocation(program_, "y_texture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, vpx_image_->d_w / 2,
                 vpx_image_->d_h / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 vpx_image_->planes[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(glGetUniformLocation(program_, "u_texture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, vpx_image_->d_w / 2,
                 vpx_image_->d_h / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 vpx_image_->planes[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(glGetUniformLocation(program_, "v_texture"), 2);

    GLfloat vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat texCoords[] = {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    GLint pos_loc = glGetAttribLocation(program_, "a_position");
    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 0, vertices);

    GLint tex_loc = glGetAttribLocation(program_, "a_texCoord");
    glEnableVertexAttribArray(tex_loc);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 0, texCoords);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  surface_->SwapBuffers(base::DoNothing(), gfx::FrameData());
}

void SplashPlayer::Stop() {
  if (vpx_codec_.name) {
    vpx_codec_destroy(&vpx_codec_);
  }
  delete segment_;
  delete reader_;
  if (program_) {
    glDeleteProgram(program_);
  }
  if (vertex_shader_) {
    glDeleteShader(vertex_shader_);
  }
  if (fragment_shader_) {
    glDeleteShader(fragment_shader_);
  }
  if (y_texture_) {
    glDeleteTextures(1, &y_texture_);
  }
  if (u_texture_) {
    glDeleteTextures(1, &u_texture_);
  }
  if (v_texture_) {
    glDeleteTextures(1, &v_texture_);
  }

  if (context_) {
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
  }
  surface_ = nullptr;
}

}  // namespace splash
}  // namespace cobalt
