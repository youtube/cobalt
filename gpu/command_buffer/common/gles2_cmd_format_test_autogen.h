// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file contains unit tests for gles2 commands
// It is included by gles2_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(GLES2FormatTest, ActiveTexture) {
  cmds::ActiveTexture& cmd = *GetBufferAs<cmds::ActiveTexture>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ActiveTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, AttachShader) {
  cmds::AttachShader& cmd = *GetBufferAs<cmds::AttachShader>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::AttachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindAttribLocationBucket) {
  cmds::BindAttribLocationBucket& cmd =
      *GetBufferAs<cmds::BindAttribLocationBucket>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindAttribLocationBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindBuffer) {
  cmds::BindBuffer& cmd = *GetBufferAs<cmds::BindBuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.buffer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindBufferBase) {
  cmds::BindBufferBase& cmd = *GetBufferAs<cmds::BindBufferBase>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindBufferBase::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.buffer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindBufferRange) {
  cmds::BindBufferRange& cmd = *GetBufferAs<cmds::BindBufferRange>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<GLintptr>(14),
              static_cast<GLsizeiptr>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindBufferRange::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.buffer);
  EXPECT_EQ(static_cast<GLintptr>(14), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(15), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindFramebuffer) {
  cmds::BindFramebuffer& cmd = *GetBufferAs<cmds::BindFramebuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.framebuffer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindRenderbuffer) {
  cmds::BindRenderbuffer& cmd = *GetBufferAs<cmds::BindRenderbuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.renderbuffer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindSampler) {
  cmds::BindSampler& cmd = *GetBufferAs<cmds::BindSampler>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindSampler::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.unit);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.sampler);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindTexture) {
  cmds::BindTexture& cmd = *GetBufferAs<cmds::BindTexture>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindTransformFeedback) {
  cmds::BindTransformFeedback& cmd =
      *GetBufferAs<cmds::BindTransformFeedback>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindTransformFeedback::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.transformfeedback);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendColor) {
  cmds::BlendColor& cmd = *GetBufferAs<cmds::BlendColor>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLclampf>(11), static_cast<GLclampf>(12),
              static_cast<GLclampf>(13), static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquation) {
  cmds::BlendEquation& cmd = *GetBufferAs<cmds::BlendEquation>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendEquation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquationSeparate) {
  cmds::BlendEquationSeparate& cmd =
      *GetBufferAs<cmds::BlendEquationSeparate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendEquationSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.modeRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.modeAlpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFunc) {
  cmds::BlendFunc& cmd = *GetBufferAs<cmds::BlendFunc>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendFunc::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.sfactor);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dfactor);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFuncSeparate) {
  cmds::BlendFuncSeparate& cmd = *GetBufferAs<cmds::BlendFuncSeparate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.srcRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dstRGB);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.srcAlpha);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.dstAlpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BufferData) {
  cmds::BufferData& cmd = *GetBufferAs<cmds::BufferData>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLsizeiptr>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<GLenum>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BufferData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizeiptr>(12), cmd.size);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.usage);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BufferSubData) {
  cmds::BufferSubData& cmd = *GetBufferAs<cmds::BufferSubData>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLintptr>(12),
              static_cast<GLsizeiptr>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BufferSubData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLintptr>(12), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.size);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CheckFramebufferStatus) {
  cmds::CheckFramebufferStatus& cmd =
      *GetBufferAs<cmds::CheckFramebufferStatus>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CheckFramebufferStatus::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Clear) {
  cmds::Clear& cmd = *GetBufferAs<cmds::Clear>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLbitfield>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Clear::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLbitfield>(11), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearBufferfi) {
  cmds::ClearBufferfi& cmd = *GetBufferAs<cmds::ClearBufferfi>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLfloat>(13), static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearBufferfi::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.buffer);
  EXPECT_EQ(static_cast<GLint>(12), cmd.drawbuffers);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.depth);
  EXPECT_EQ(static_cast<GLint>(14), cmd.stencil);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearBufferfvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  cmds::ClearBufferfvImmediate& cmd =
      *GetBufferAs<cmds::ClearBufferfvImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearBufferfvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.buffer);
  EXPECT_EQ(static_cast<GLint>(12), cmd.drawbuffers);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, ClearBufferivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::ClearBufferivImmediate& cmd =
      *GetBufferAs<cmds::ClearBufferivImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearBufferivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.buffer);
  EXPECT_EQ(static_cast<GLint>(12), cmd.drawbuffers);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, ClearBufferuivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::ClearBufferuivImmediate& cmd =
      *GetBufferAs<cmds::ClearBufferuivImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearBufferuivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.buffer);
  EXPECT_EQ(static_cast<GLint>(12), cmd.drawbuffers);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, ClearColor) {
  cmds::ClearColor& cmd = *GetBufferAs<cmds::ClearColor>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLclampf>(11), static_cast<GLclampf>(12),
              static_cast<GLclampf>(13), static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearDepthf) {
  cmds::ClearDepthf& cmd = *GetBufferAs<cmds::ClearDepthf>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLclampf>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearDepthf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.depth);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearStencil) {
  cmds::ClearStencil& cmd = *GetBufferAs<cmds::ClearStencil>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearStencil::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.s);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClientWaitSync) {
  cmds::ClientWaitSync& cmd = *GetBufferAs<cmds::ClientWaitSync>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLbitfield>(12),
              static_cast<GLuint64>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClientWaitSync::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sync);
  EXPECT_EQ(static_cast<GLbitfield>(12), cmd.flags);
  EXPECT_EQ(static_cast<GLuint64>(13), cmd.timeout());
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ColorMask) {
  cmds::ColorMask& cmd = *GetBufferAs<cmds::ColorMask>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLboolean>(11), static_cast<GLboolean>(12),
              static_cast<GLboolean>(13), static_cast<GLboolean>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ColorMask::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.red);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.green);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompileShader) {
  cmds::CompileShader& cmd = *GetBufferAs<cmds::CompileShader>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompileShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage2DBucket) {
  cmds::CompressedTexImage2DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexImage2DBucket>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLuint>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage2D) {
  cmds::CompressedTexImage2D& cmd = *GetBufferAs<cmds::CompressedTexImage2D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16),
              static_cast<uint32_t>(17), static_cast<uint32_t>(18));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage2DBucket) {
  cmds::CompressedTexSubImage2DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexSubImage2DBucket>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16),
              static_cast<GLenum>(17), static_cast<GLuint>(18));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexSubImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage2D) {
  cmds::CompressedTexSubImage2D& cmd =
      *GetBufferAs<cmds::CompressedTexSubImage2D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16),
              static_cast<GLenum>(17), static_cast<GLsizei>(18),
              static_cast<uint32_t>(19), static_cast<uint32_t>(20));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(20), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage3DBucket) {
  cmds::CompressedTexImage3DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexImage3DBucket>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLint>(12), static_cast<GLenum>(13),
                           static_cast<GLsizei>(14), static_cast<GLsizei>(15),
                           static_cast<GLsizei>(16), static_cast<GLuint>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexImage3DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.depth);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage3D) {
  cmds::CompressedTexImage3D& cmd = *GetBufferAs<cmds::CompressedTexImage3D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16),
              static_cast<GLsizei>(17), static_cast<uint32_t>(18),
              static_cast<uint32_t>(19));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexImage3D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.depth);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage3DBucket) {
  cmds::CompressedTexSubImage3DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexSubImage3DBucket>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLint>(15), static_cast<GLsizei>(16),
              static_cast<GLsizei>(17), static_cast<GLsizei>(18),
              static_cast<GLenum>(19), static_cast<GLuint>(20));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexSubImage3DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.zoffset);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.depth);
  EXPECT_EQ(static_cast<GLenum>(19), cmd.format);
  EXPECT_EQ(static_cast<GLuint>(20), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage3D) {
  cmds::CompressedTexSubImage3D& cmd =
      *GetBufferAs<cmds::CompressedTexSubImage3D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLint>(15), static_cast<GLsizei>(16),
              static_cast<GLsizei>(17), static_cast<GLsizei>(18),
              static_cast<GLenum>(19), static_cast<GLsizei>(20),
              static_cast<uint32_t>(21), static_cast<uint32_t>(22));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexSubImage3D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.zoffset);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.depth);
  EXPECT_EQ(static_cast<GLenum>(19), cmd.format);
  EXPECT_EQ(static_cast<GLsizei>(20), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32_t>(21), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(22), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyBufferSubData) {
  cmds::CopyBufferSubData& cmd = *GetBufferAs<cmds::CopyBufferSubData>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLintptr>(13), static_cast<GLintptr>(14),
              static_cast<GLsizeiptr>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopyBufferSubData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.readtarget);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.writetarget);
  EXPECT_EQ(static_cast<GLintptr>(13), cmd.readoffset);
  EXPECT_EQ(static_cast<GLintptr>(14), cmd.writeoffset);
  EXPECT_EQ(static_cast<GLsizeiptr>(15), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexImage2D) {
  cmds::CopyTexImage2D& cmd = *GetBufferAs<cmds::CopyTexImage2D>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLint>(12), static_cast<GLenum>(13),
                           static_cast<GLint>(14), static_cast<GLint>(15),
                           static_cast<GLsizei>(16), static_cast<GLsizei>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopyTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLint>(14), cmd.x);
  EXPECT_EQ(static_cast<GLint>(15), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexSubImage2D) {
  cmds::CopyTexSubImage2D& cmd = *GetBufferAs<cmds::CopyTexSubImage2D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLint>(15), static_cast<GLint>(16),
              static_cast<GLsizei>(17), static_cast<GLsizei>(18));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopyTexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexSubImage3D) {
  cmds::CopyTexSubImage3D& cmd = *GetBufferAs<cmds::CopyTexSubImage3D>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLint>(12), static_cast<GLint>(13),
                           static_cast<GLint>(14), static_cast<GLint>(15),
                           static_cast<GLint>(16), static_cast<GLint>(17),
                           static_cast<GLsizei>(18), static_cast<GLsizei>(19));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopyTexSubImage3D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.zoffset);
  EXPECT_EQ(static_cast<GLint>(16), cmd.x);
  EXPECT_EQ(static_cast<GLint>(17), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(19), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateProgram) {
  cmds::CreateProgram& cmd = *GetBufferAs<cmds::CreateProgram>();
  void* next_cmd = cmd.Set(&cmd, static_cast<uint32_t>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CreateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateShader) {
  cmds::CreateShader& cmd = *GetBufferAs<cmds::CreateShader>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CreateShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CullFace) {
  cmds::CullFace& cmd = *GetBufferAs<cmds::CullFace>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CullFace::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteBuffersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteBuffersImmediate& cmd =
      *GetBufferAs<cmds::DeleteBuffersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteFramebuffersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteFramebuffersImmediate& cmd =
      *GetBufferAs<cmds::DeleteFramebuffersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteProgram) {
  cmds::DeleteProgram& cmd = *GetBufferAs<cmds::DeleteProgram>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteRenderbuffersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteRenderbuffersImmediate& cmd =
      *GetBufferAs<cmds::DeleteRenderbuffersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteSamplersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteSamplersImmediate& cmd =
      *GetBufferAs<cmds::DeleteSamplersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteSamplersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteSync) {
  cmds::DeleteSync& cmd = *GetBufferAs<cmds::DeleteSync>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteSync::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sync);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteShader) {
  cmds::DeleteShader& cmd = *GetBufferAs<cmds::DeleteShader>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteTexturesImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteTexturesImmediate& cmd =
      *GetBufferAs<cmds::DeleteTexturesImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteTransformFeedbacksImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteTransformFeedbacksImmediate& cmd =
      *GetBufferAs<cmds::DeleteTransformFeedbacksImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::DeleteTransformFeedbacksImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DepthFunc) {
  cmds::DepthFunc& cmd = *GetBufferAs<cmds::DepthFunc>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DepthFunc::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DepthMask) {
  cmds::DepthMask& cmd = *GetBufferAs<cmds::DepthMask>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLboolean>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DepthMask::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.flag);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DepthRangef) {
  cmds::DepthRangef& cmd = *GetBufferAs<cmds::DepthRangef>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLclampf>(11), static_cast<GLclampf>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DepthRangef::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.zNear);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.zFar);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DetachShader) {
  cmds::DetachShader& cmd = *GetBufferAs<cmds::DetachShader>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DetachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Disable) {
  cmds::Disable& cmd = *GetBufferAs<cmds::Disable>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Disable::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DisableVertexAttribArray) {
  cmds::DisableVertexAttribArray& cmd =
      *GetBufferAs<cmds::DisableVertexAttribArray>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DisableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArrays) {
  cmds::DrawArrays& cmd = *GetBufferAs<cmds::DrawArrays>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLint>(12), static_cast<GLsizei>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawArrays::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElements) {
  cmds::DrawElements& cmd = *GetBufferAs<cmds::DrawElements>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLsizei>(12),
              static_cast<GLenum>(13), static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawElements::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Enable) {
  cmds::Enable& cmd = *GetBufferAs<cmds::Enable>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Enable::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableVertexAttribArray) {
  cmds::EnableVertexAttribArray& cmd =
      *GetBufferAs<cmds::EnableVertexAttribArray>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::EnableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FenceSync) {
  cmds::FenceSync& cmd = *GetBufferAs<cmds::FenceSync>();
  void* next_cmd = cmd.Set(&cmd, static_cast<uint32_t>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FenceSync::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Finish) {
  cmds::Finish& cmd = *GetBufferAs<cmds::Finish>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Finish::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Flush) {
  cmds::Flush& cmd = *GetBufferAs<cmds::Flush>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Flush::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferRenderbuffer) {
  cmds::FramebufferRenderbuffer& cmd =
      *GetBufferAs<cmds::FramebufferRenderbuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FramebufferRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.renderbuffertarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.renderbuffer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTexture2D) {
  cmds::FramebufferTexture2D& cmd = *GetBufferAs<cmds::FramebufferTexture2D>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLenum>(13),
                           static_cast<GLuint>(14), static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FramebufferTexture2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.textarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(15), cmd.level);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTextureLayer) {
  cmds::FramebufferTextureLayer& cmd =
      *GetBufferAs<cmds::FramebufferTextureLayer>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLuint>(13),
                           static_cast<GLint>(14), static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FramebufferTextureLayer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(14), cmd.level);
  EXPECT_EQ(static_cast<GLint>(15), cmd.layer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FrontFace) {
  cmds::FrontFace& cmd = *GetBufferAs<cmds::FrontFace>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FrontFace::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenBuffersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenBuffersImmediate& cmd = *GetBufferAs<cmds::GenBuffersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, GenerateMipmap) {
  cmds::GenerateMipmap& cmd = *GetBufferAs<cmds::GenerateMipmap>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenerateMipmap::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenFramebuffersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenFramebuffersImmediate& cmd =
      *GetBufferAs<cmds::GenFramebuffersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, GenRenderbuffersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenRenderbuffersImmediate& cmd =
      *GetBufferAs<cmds::GenRenderbuffersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, GenSamplersImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenSamplersImmediate& cmd = *GetBufferAs<cmds::GenSamplersImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenSamplersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, GenTexturesImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenTexturesImmediate& cmd = *GetBufferAs<cmds::GenTexturesImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, GenTransformFeedbacksImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenTransformFeedbacksImmediate& cmd =
      *GetBufferAs<cmds::GenTransformFeedbacksImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenTransformFeedbacksImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, GetActiveAttrib) {
  cmds::GetActiveAttrib& cmd = *GetBufferAs<cmds::GetActiveAttrib>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetActiveAttrib::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetActiveUniform) {
  cmds::GetActiveUniform& cmd = *GetBufferAs<cmds::GetActiveUniform>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetActiveUniform::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetActiveUniformBlockiv) {
  cmds::GetActiveUniformBlockiv& cmd =
      *GetBufferAs<cmds::GetActiveUniformBlockiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLenum>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetActiveUniformBlockiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetActiveUniformBlockName) {
  cmds::GetActiveUniformBlockName& cmd =
      *GetBufferAs<cmds::GetActiveUniformBlockName>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetActiveUniformBlockName::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetActiveUniformsiv) {
  cmds::GetActiveUniformsiv& cmd = *GetBufferAs<cmds::GetActiveUniformsiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<GLenum>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetActiveUniformsiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.indices_bucket_id);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetAttachedShaders) {
  cmds::GetAttachedShaders& cmd = *GetBufferAs<cmds::GetAttachedShaders>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetAttachedShaders::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetAttribLocation) {
  cmds::GetAttribLocation& cmd = *GetBufferAs<cmds::GetAttribLocation>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetAttribLocation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.location_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.location_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetBooleanv) {
  cmds::GetBooleanv& cmd = *GetBufferAs<cmds::GetBooleanv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetBooleanv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetBooleani_v) {
  cmds::GetBooleani_v& cmd = *GetBufferAs<cmds::GetBooleani_v>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetBooleani_v::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetBufferParameteri64v) {
  cmds::GetBufferParameteri64v& cmd =
      *GetBufferAs<cmds::GetBufferParameteri64v>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetBufferParameteri64v::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetBufferParameteriv) {
  cmds::GetBufferParameteriv& cmd = *GetBufferAs<cmds::GetBufferParameteriv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetBufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetError) {
  cmds::GetError& cmd = *GetBufferAs<cmds::GetError>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint32_t>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetError::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFloatv) {
  cmds::GetFloatv& cmd = *GetBufferAs<cmds::GetFloatv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetFloatv::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFragDataLocation) {
  cmds::GetFragDataLocation& cmd = *GetBufferAs<cmds::GetFragDataLocation>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetFragDataLocation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.location_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.location_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFramebufferAttachmentParameteriv) {
  cmds::GetFramebufferAttachmentParameteriv& cmd =
      *GetBufferAs<cmds::GetFramebufferAttachmentParameteriv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::GetFramebufferAttachmentParameteriv::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetInteger64v) {
  cmds::GetInteger64v& cmd = *GetBufferAs<cmds::GetInteger64v>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetInteger64v::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetIntegeri_v) {
  cmds::GetIntegeri_v& cmd = *GetBufferAs<cmds::GetIntegeri_v>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetIntegeri_v::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetInteger64i_v) {
  cmds::GetInteger64i_v& cmd = *GetBufferAs<cmds::GetInteger64i_v>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetInteger64i_v::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetIntegerv) {
  cmds::GetIntegerv& cmd = *GetBufferAs<cmds::GetIntegerv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetIntegerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetInternalformativ) {
  cmds::GetInternalformativ& cmd = *GetBufferAs<cmds::GetInternalformativ>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetInternalformativ::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramiv) {
  cmds::GetProgramiv& cmd = *GetBufferAs<cmds::GetProgramiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInfoLog) {
  cmds::GetProgramInfoLog& cmd = *GetBufferAs<cmds::GetProgramInfoLog>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetRenderbufferParameteriv) {
  cmds::GetRenderbufferParameteriv& cmd =
      *GetBufferAs<cmds::GetRenderbufferParameteriv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetRenderbufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetSamplerParameterfv) {
  cmds::GetSamplerParameterfv& cmd =
      *GetBufferAs<cmds::GetSamplerParameterfv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetSamplerParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetSamplerParameteriv) {
  cmds::GetSamplerParameteriv& cmd =
      *GetBufferAs<cmds::GetSamplerParameteriv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetSamplerParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderiv) {
  cmds::GetShaderiv& cmd = *GetBufferAs<cmds::GetShaderiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetShaderiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderInfoLog) {
  cmds::GetShaderInfoLog& cmd = *GetBufferAs<cmds::GetShaderInfoLog>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetShaderInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderPrecisionFormat) {
  cmds::GetShaderPrecisionFormat& cmd =
      *GetBufferAs<cmds::GetShaderPrecisionFormat>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetShaderPrecisionFormat::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.shadertype);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.precisiontype);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderSource) {
  cmds::GetShaderSource& cmd = *GetBufferAs<cmds::GetShaderSource>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetString) {
  cmds::GetString& cmd = *GetBufferAs<cmds::GetString>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetString::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.name);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetSynciv) {
  cmds::GetSynciv& cmd = *GetBufferAs<cmds::GetSynciv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetSynciv::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sync);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTexParameterfv) {
  cmds::GetTexParameterfv& cmd = *GetBufferAs<cmds::GetTexParameterfv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetTexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTexParameteriv) {
  cmds::GetTexParameteriv& cmd = *GetBufferAs<cmds::GetTexParameteriv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetTexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTransformFeedbackVarying) {
  cmds::GetTransformFeedbackVarying& cmd =
      *GetBufferAs<cmds::GetTransformFeedbackVarying>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetTransformFeedbackVarying::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformBlockIndex) {
  cmds::GetUniformBlockIndex& cmd = *GetBufferAs<cmds::GetUniformBlockIndex>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformBlockIndex::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.index_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.index_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformfv) {
  cmds::GetUniformfv& cmd = *GetBufferAs<cmds::GetUniformfv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformiv) {
  cmds::GetUniformiv& cmd = *GetBufferAs<cmds::GetUniformiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformuiv) {
  cmds::GetUniformuiv& cmd = *GetBufferAs<cmds::GetUniformuiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformuiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformIndices) {
  cmds::GetUniformIndices& cmd = *GetBufferAs<cmds::GetUniformIndices>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformIndices::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.names_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.indices_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.indices_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformLocation) {
  cmds::GetUniformLocation& cmd = *GetBufferAs<cmds::GetUniformLocation>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformLocation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.location_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.location_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribfv) {
  cmds::GetVertexAttribfv& cmd = *GetBufferAs<cmds::GetVertexAttribfv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetVertexAttribfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribiv) {
  cmds::GetVertexAttribiv& cmd = *GetBufferAs<cmds::GetVertexAttribiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetVertexAttribiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribIiv) {
  cmds::GetVertexAttribIiv& cmd = *GetBufferAs<cmds::GetVertexAttribIiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetVertexAttribIiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribIuiv) {
  cmds::GetVertexAttribIuiv& cmd = *GetBufferAs<cmds::GetVertexAttribIuiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetVertexAttribIuiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribPointerv) {
  cmds::GetVertexAttribPointerv& cmd =
      *GetBufferAs<cmds::GetVertexAttribPointerv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetVertexAttribPointerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.pointer_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.pointer_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Hint) {
  cmds::Hint& cmd = *GetBufferAs<cmds::Hint>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Hint::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, InvalidateFramebufferImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
      static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
      static_cast<GLenum>(kSomeBaseValueToTestWith + 1),
  };
  cmds::InvalidateFramebufferImmediate& cmd =
      *GetBufferAs<cmds::InvalidateFramebufferImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::InvalidateFramebufferImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(1), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, InvalidateSubFramebufferImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
      static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
      static_cast<GLenum>(kSomeBaseValueToTestWith + 1),
  };
  cmds::InvalidateSubFramebufferImmediate& cmd =
      *GetBufferAs<cmds::InvalidateSubFramebufferImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(1), static_cast<GLsizei>(2), data,
              static_cast<GLint>(4), static_cast<GLint>(5),
              static_cast<GLsizei>(6), static_cast<GLsizei>(7));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::InvalidateSubFramebufferImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(1), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLint>(4), cmd.x);
  EXPECT_EQ(static_cast<GLint>(5), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(6), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(7), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, IsBuffer) {
  cmds::IsBuffer& cmd = *GetBufferAs<cmds::IsBuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsBuffer::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsEnabled) {
  cmds::IsEnabled& cmd = *GetBufferAs<cmds::IsEnabled>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsEnabled::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsFramebuffer) {
  cmds::IsFramebuffer& cmd = *GetBufferAs<cmds::IsFramebuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.framebuffer);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsProgram) {
  cmds::IsProgram& cmd = *GetBufferAs<cmds::IsProgram>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsProgram::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsRenderbuffer) {
  cmds::IsRenderbuffer& cmd = *GetBufferAs<cmds::IsRenderbuffer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.renderbuffer);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsSampler) {
  cmds::IsSampler& cmd = *GetBufferAs<cmds::IsSampler>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsSampler::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsShader) {
  cmds::IsShader& cmd = *GetBufferAs<cmds::IsShader>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsShader::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsSync) {
  cmds::IsSync& cmd = *GetBufferAs<cmds::IsSync>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsSync::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sync);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsTexture) {
  cmds::IsTexture& cmd = *GetBufferAs<cmds::IsTexture>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsTexture::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsTransformFeedback) {
  cmds::IsTransformFeedback& cmd = *GetBufferAs<cmds::IsTransformFeedback>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsTransformFeedback::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.transformfeedback);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LineWidth) {
  cmds::LineWidth& cmd = *GetBufferAs<cmds::LineWidth>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLfloat>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LineWidth::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.width);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LinkProgram) {
  cmds::LinkProgram& cmd = *GetBufferAs<cmds::LinkProgram>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LinkProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PauseTransformFeedback) {
  cmds::PauseTransformFeedback& cmd =
      *GetBufferAs<cmds::PauseTransformFeedback>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::PauseTransformFeedback::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PixelStorei) {
  cmds::PixelStorei& cmd = *GetBufferAs<cmds::PixelStorei>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::PixelStorei::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(12), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PolygonOffset) {
  cmds::PolygonOffset& cmd = *GetBufferAs<cmds::PolygonOffset>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLfloat>(11), static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::PolygonOffset::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.factor);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.units);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReadBuffer) {
  cmds::ReadBuffer& cmd = *GetBufferAs<cmds::ReadBuffer>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ReadBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.src);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReadPixels) {
  cmds::ReadPixels& cmd = *GetBufferAs<cmds::ReadPixels>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLsizei>(13), static_cast<GLsizei>(14),
                           static_cast<GLenum>(15), static_cast<GLenum>(16),
                           static_cast<uint32_t>(17), static_cast<uint32_t>(18),
                           static_cast<uint32_t>(19), static_cast<uint32_t>(20),
                           static_cast<GLboolean>(21));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ReadPixels::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(20), cmd.result_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(21), cmd.async);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReleaseShaderCompiler) {
  cmds::ReleaseShaderCompiler& cmd =
      *GetBufferAs<cmds::ReleaseShaderCompiler>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ReleaseShaderCompiler::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorage) {
  cmds::RenderbufferStorage& cmd = *GetBufferAs<cmds::RenderbufferStorage>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLsizei>(13), static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::RenderbufferStorage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ResumeTransformFeedback) {
  cmds::ResumeTransformFeedback& cmd =
      *GetBufferAs<cmds::ResumeTransformFeedback>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ResumeTransformFeedback::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SampleCoverage) {
  cmds::SampleCoverage& cmd = *GetBufferAs<cmds::SampleCoverage>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLclampf>(11), static_cast<GLboolean>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SampleCoverage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.value);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.invert);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SamplerParameterf) {
  cmds::SamplerParameterf& cmd = *GetBufferAs<cmds::SamplerParameterf>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SamplerParameterf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SamplerParameterfvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  cmds::SamplerParameterfvImmediate& cmd =
      *GetBufferAs<cmds::SamplerParameterfvImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::SamplerParameterfvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, SamplerParameteri) {
  cmds::SamplerParameteri& cmd = *GetBufferAs<cmds::SamplerParameteri>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SamplerParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SamplerParameterivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
  };
  cmds::SamplerParameterivImmediate& cmd =
      *GetBufferAs<cmds::SamplerParameterivImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::SamplerParameterivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sampler);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Scissor) {
  cmds::Scissor& cmd = *GetBufferAs<cmds::Scissor>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLsizei>(13), static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Scissor::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderBinary) {
  cmds::ShaderBinary& cmd = *GetBufferAs<cmds::ShaderBinary>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(11),
                           static_cast<uint32_t>(12), static_cast<uint32_t>(13),
                           static_cast<GLenum>(14), static_cast<uint32_t>(15),
                           static_cast<uint32_t>(16), static_cast<GLsizei>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ShaderBinary::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.shaders_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.shaders_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.binaryformat);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.binary_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.binary_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.length);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderSourceBucket) {
  cmds::ShaderSourceBucket& cmd = *GetBufferAs<cmds::ShaderSourceBucket>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ShaderSourceBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.str_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawBeginCHROMIUM) {
  cmds::MultiDrawBeginCHROMIUM& cmd =
      *GetBufferAs<cmds::MultiDrawBeginCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MultiDrawBeginCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawEndCHROMIUM) {
  cmds::MultiDrawEndCHROMIUM& cmd = *GetBufferAs<cmds::MultiDrawEndCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::MultiDrawEndCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawArraysCHROMIUM) {
  cmds::MultiDrawArraysCHROMIUM& cmd =
      *GetBufferAs<cmds::MultiDrawArraysCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15), static_cast<GLsizei>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MultiDrawArraysCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.firsts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.firsts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.counts_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawArraysInstancedCHROMIUM) {
  cmds::MultiDrawArraysInstancedCHROMIUM& cmd =
      *GetBufferAs<cmds::MultiDrawArraysInstancedCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16),
              static_cast<uint32_t>(17), static_cast<GLsizei>(18));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::MultiDrawArraysInstancedCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.firsts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.firsts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.counts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.instance_counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.instance_counts_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawArraysInstancedBaseInstanceCHROMIUM) {
  cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM& cmd =
      *GetBufferAs<cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16),
              static_cast<uint32_t>(17), static_cast<uint32_t>(18),
              static_cast<uint32_t>(19), static_cast<GLsizei>(20));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.firsts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.firsts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.counts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.instance_counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.instance_counts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.baseinstances_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.baseinstances_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(20), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawElementsCHROMIUM) {
  cmds::MultiDrawElementsCHROMIUM& cmd =
      *GetBufferAs<cmds::MultiDrawElementsCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<uint32_t>(12), static_cast<uint32_t>(13),
                           static_cast<GLenum>(14), static_cast<uint32_t>(15),
                           static_cast<uint32_t>(16), static_cast<GLsizei>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MultiDrawElementsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.counts_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.offsets_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.offsets_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MultiDrawElementsInstancedCHROMIUM) {
  cmds::MultiDrawElementsInstancedCHROMIUM& cmd =
      *GetBufferAs<cmds::MultiDrawElementsInstancedCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<uint32_t>(12), static_cast<uint32_t>(13),
                           static_cast<GLenum>(14), static_cast<uint32_t>(15),
                           static_cast<uint32_t>(16), static_cast<uint32_t>(17),
                           static_cast<uint32_t>(18), static_cast<GLsizei>(19));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::MultiDrawElementsInstancedCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.counts_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.offsets_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.offsets_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.instance_counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.instance_counts_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(19), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest,
       MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM) {
  cmds::MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM& cmd =
      *GetBufferAs<
          cmds::MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<uint32_t>(12), static_cast<uint32_t>(13),
                           static_cast<GLenum>(14), static_cast<uint32_t>(15),
                           static_cast<uint32_t>(16), static_cast<uint32_t>(17),
                           static_cast<uint32_t>(18), static_cast<uint32_t>(19),
                           static_cast<uint32_t>(20), static_cast<uint32_t>(21),
                           static_cast<uint32_t>(22), static_cast<GLsizei>(23));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM::
                    kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.counts_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.offsets_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.offsets_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.instance_counts_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.instance_counts_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.basevertices_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(20), cmd.basevertices_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(21), cmd.baseinstances_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(22), cmd.baseinstances_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(23), cmd.drawcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilFunc) {
  cmds::StencilFunc& cmd = *GetBufferAs<cmds::StencilFunc>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLint>(12), static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::StencilFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  EXPECT_EQ(static_cast<GLint>(12), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilFuncSeparate) {
  cmds::StencilFuncSeparate& cmd = *GetBufferAs<cmds::StencilFuncSeparate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLint>(13), static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::StencilFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.func);
  EXPECT_EQ(static_cast<GLint>(13), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilMask) {
  cmds::StencilMask& cmd = *GetBufferAs<cmds::StencilMask>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::StencilMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilMaskSeparate) {
  cmds::StencilMaskSeparate& cmd = *GetBufferAs<cmds::StencilMaskSeparate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::StencilMaskSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilOp) {
  cmds::StencilOp& cmd = *GetBufferAs<cmds::StencilOp>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLenum>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::StencilOp::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zpass);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilOpSeparate) {
  cmds::StencilOpSeparate& cmd = *GetBufferAs<cmds::StencilOpSeparate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::StencilOpSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.zpass);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexImage2D) {
  cmds::TexImage2D& cmd = *GetBufferAs<cmds::TexImage2D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLenum>(16),
              static_cast<GLenum>(17), static_cast<uint32_t>(18),
              static_cast<uint32_t>(19));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.pixels_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexImage3D) {
  cmds::TexImage3D& cmd = *GetBufferAs<cmds::TexImage3D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16),
              static_cast<GLenum>(17), static_cast<GLenum>(18),
              static_cast<uint32_t>(19), static_cast<uint32_t>(20));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexImage3D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.depth);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(20), cmd.pixels_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterf) {
  cmds::TexParameterf& cmd = *GetBufferAs<cmds::TexParameterf>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexParameterf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterfvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  cmds::TexParameterfvImmediate& cmd =
      *GetBufferAs<cmds::TexParameterfvImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexParameterfvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, TexParameteri) {
  cmds::TexParameteri& cmd = *GetBufferAs<cmds::TexParameteri>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
  };
  cmds::TexParameterivImmediate& cmd =
      *GetBufferAs<cmds::TexParameterivImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexParameterivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, TexStorage3D) {
  cmds::TexStorage3D& cmd = *GetBufferAs<cmds::TexStorage3D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLsizei>(12),
              static_cast<GLenum>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexStorage3D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.levels);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalFormat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.depth);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexSubImage2D) {
  cmds::TexSubImage2D& cmd = *GetBufferAs<cmds::TexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
      static_cast<GLint>(13), static_cast<GLint>(14), static_cast<GLsizei>(15),
      static_cast<GLsizei>(16), static_cast<GLenum>(17),
      static_cast<GLenum>(18), static_cast<uint32_t>(19),
      static_cast<uint32_t>(20), static_cast<GLboolean>(21));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(20), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(21), cmd.internal);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexSubImage3D) {
  cmds::TexSubImage3D& cmd = *GetBufferAs<cmds::TexSubImage3D>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
      static_cast<GLint>(13), static_cast<GLint>(14), static_cast<GLint>(15),
      static_cast<GLsizei>(16), static_cast<GLsizei>(17),
      static_cast<GLsizei>(18), static_cast<GLenum>(19),
      static_cast<GLenum>(20), static_cast<uint32_t>(21),
      static_cast<uint32_t>(22), static_cast<GLboolean>(23));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexSubImage3D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.zoffset);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.depth);
  EXPECT_EQ(static_cast<GLenum>(19), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(20), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(21), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(22), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(23), cmd.internal);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TransformFeedbackVaryingsBucket) {
  cmds::TransformFeedbackVaryingsBucket& cmd =
      *GetBufferAs<cmds::TransformFeedbackVaryingsBucket>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<uint32_t>(12), static_cast<GLenum>(13));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::TransformFeedbackVaryingsBucket::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.varyings_bucket_id);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.buffermode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1f) {
  cmds::Uniform1f& cmd = *GetBufferAs<cmds::Uniform1f>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform1f::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  cmds::Uniform1fvImmediate& cmd = *GetBufferAs<cmds::Uniform1fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 1;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform1i) {
  cmds::Uniform1i& cmd = *GetBufferAs<cmds::Uniform1i>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform1i::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
  };
  cmds::Uniform1ivImmediate& cmd = *GetBufferAs<cmds::Uniform1ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 1;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform1ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform1ui) {
  cmds::Uniform1ui& cmd = *GetBufferAs<cmds::Uniform1ui>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform1ui::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1uivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
  };
  cmds::Uniform1uivImmediate& cmd = *GetBufferAs<cmds::Uniform1uivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLuint) * 1;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform1uivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform2f) {
  cmds::Uniform2f& cmd = *GetBufferAs<cmds::Uniform2f>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11),
                           static_cast<GLfloat>(12), static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform2f::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  cmds::Uniform2fvImmediate& cmd = *GetBufferAs<cmds::Uniform2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 2;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform2i) {
  cmds::Uniform2i& cmd = *GetBufferAs<cmds::Uniform2i>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform2i::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::Uniform2ivImmediate& cmd = *GetBufferAs<cmds::Uniform2ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 2;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform2ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform2ui) {
  cmds::Uniform2ui& cmd = *GetBufferAs<cmds::Uniform2ui>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform2ui::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2uivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::Uniform2uivImmediate& cmd = *GetBufferAs<cmds::Uniform2uivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLuint) * 2;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform2uivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform3f) {
  cmds::Uniform3f& cmd = *GetBufferAs<cmds::Uniform3f>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLfloat>(12),
              static_cast<GLfloat>(13), static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform3f::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
  };
  cmds::Uniform3fvImmediate& cmd = *GetBufferAs<cmds::Uniform3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 3;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform3i) {
  cmds::Uniform3i& cmd = *GetBufferAs<cmds::Uniform3i>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLint>(13), static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform3i::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
      static_cast<GLint>(kSomeBaseValueToTestWith + 4),
      static_cast<GLint>(kSomeBaseValueToTestWith + 5),
  };
  cmds::Uniform3ivImmediate& cmd = *GetBufferAs<cmds::Uniform3ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 3;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform3ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform3ui) {
  cmds::Uniform3ui& cmd = *GetBufferAs<cmds::Uniform3ui>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform3ui::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3uivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 4),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 5),
  };
  cmds::Uniform3uivImmediate& cmd = *GetBufferAs<cmds::Uniform3uivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLuint) * 3;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform3uivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform4f) {
  cmds::Uniform4f& cmd = *GetBufferAs<cmds::Uniform4f>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11),
                           static_cast<GLfloat>(12), static_cast<GLfloat>(13),
                           static_cast<GLfloat>(14), static_cast<GLfloat>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform4f::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
  };
  cmds::Uniform4fvImmediate& cmd = *GetBufferAs<cmds::Uniform4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 4;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform4i) {
  cmds::Uniform4i& cmd = *GetBufferAs<cmds::Uniform4i>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLint>(13), static_cast<GLint>(14),
                           static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform4i::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLint>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
      static_cast<GLint>(kSomeBaseValueToTestWith + 4),
      static_cast<GLint>(kSomeBaseValueToTestWith + 5),
      static_cast<GLint>(kSomeBaseValueToTestWith + 6),
      static_cast<GLint>(kSomeBaseValueToTestWith + 7),
  };
  cmds::Uniform4ivImmediate& cmd = *GetBufferAs<cmds::Uniform4ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 4;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform4ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, Uniform4ui) {
  cmds::Uniform4ui& cmd = *GetBufferAs<cmds::Uniform4ui>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13),
                           static_cast<GLuint>(14), static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform4ui::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4uivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 4),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 5),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 6),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 7),
  };
  cmds::Uniform4uivImmediate& cmd = *GetBufferAs<cmds::Uniform4uivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLuint) * 4;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Uniform4uivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformBlockBinding) {
  cmds::UniformBlockBinding& cmd = *GetBufferAs<cmds::UniformBlockBinding>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformBlockBinding::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.binding);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
  };
  cmds::UniformMatrix2fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 4;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix2x3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
  };
  cmds::UniformMatrix2x3fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix2x3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 6;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix2x3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix2x4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
  };
  cmds::UniformMatrix2x4fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix2x4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 8;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix2x4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
  };
  cmds::UniformMatrix3fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 9;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix3x2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
  };
  cmds::UniformMatrix3x2fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix3x2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 6;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix3x2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix3x4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 18),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 19),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 20),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 21),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 22),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 23),
  };
  cmds::UniformMatrix3x4fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix3x4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 12;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix3x4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 18),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 19),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 20),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 21),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 22),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 23),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 24),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 25),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 26),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 27),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 28),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 29),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 30),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 31),
  };
  cmds::UniformMatrix4fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 16;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix4x2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
  };
  cmds::UniformMatrix4x2fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix4x2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 8;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix4x2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UniformMatrix4x3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 18),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 19),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 20),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 21),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 22),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 23),
  };
  cmds::UniformMatrix4x3fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix4x3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 12;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(1), static_cast<GLsizei>(2),
                           static_cast<GLboolean>(3), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::UniformMatrix4x3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, UseProgram) {
  cmds::UseProgram& cmd = *GetBufferAs<cmds::UseProgram>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::UseProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ValidateProgram) {
  cmds::ValidateProgram& cmd = *GetBufferAs<cmds::ValidateProgram>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ValidateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1f) {
  cmds::VertexAttrib1f& cmd = *GetBufferAs<cmds::VertexAttrib1f>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  cmds::VertexAttrib1fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib1fvImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, VertexAttrib2f) {
  cmds::VertexAttrib2f& cmd = *GetBufferAs<cmds::VertexAttrib2f>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLfloat>(12), static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  cmds::VertexAttrib2fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib2fvImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, VertexAttrib3f) {
  cmds::VertexAttrib3f& cmd = *GetBufferAs<cmds::VertexAttrib3f>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLfloat>(12),
              static_cast<GLfloat>(13), static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
  };
  cmds::VertexAttrib3fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib3fvImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, VertexAttrib4f) {
  cmds::VertexAttrib4f& cmd = *GetBufferAs<cmds::VertexAttrib4f>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLfloat>(12), static_cast<GLfloat>(13),
                           static_cast<GLfloat>(14), static_cast<GLfloat>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib4f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  cmds::VertexAttrib4fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib4fvImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttrib4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, VertexAttribI4i) {
  cmds::VertexAttribI4i& cmd = *GetBufferAs<cmds::VertexAttribI4i>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLint>(12), static_cast<GLint>(13),
                           static_cast<GLint>(14), static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribI4i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLint>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttribI4ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::VertexAttribI4ivImmediate& cmd =
      *GetBufferAs<cmds::VertexAttribI4ivImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribI4ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, VertexAttribI4ui) {
  cmds::VertexAttribI4ui& cmd = *GetBufferAs<cmds::VertexAttribI4ui>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13),
                           static_cast<GLuint>(14), static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribI4ui::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttribI4uivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::VertexAttribI4uivImmediate& cmd =
      *GetBufferAs<cmds::VertexAttribI4uivImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribI4uivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, VertexAttribIPointer) {
  cmds::VertexAttribIPointer& cmd = *GetBufferAs<cmds::VertexAttribIPointer>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLint>(12), static_cast<GLenum>(13),
                           static_cast<GLsizei>(14), static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribIPointer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLint>(12), cmd.size);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.stride);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttribPointer) {
  cmds::VertexAttribPointer& cmd = *GetBufferAs<cmds::VertexAttribPointer>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLboolean>(14),
              static_cast<GLsizei>(15), static_cast<GLuint>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribPointer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLint>(12), cmd.size);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.normalized);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.stride);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Viewport) {
  cmds::Viewport& cmd = *GetBufferAs<cmds::Viewport>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLsizei>(13), static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::Viewport::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, WaitSync) {
  cmds::WaitSync& cmd = *GetBufferAs<cmds::WaitSync>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLbitfield>(12),
              static_cast<GLuint64>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::WaitSync::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sync);
  EXPECT_EQ(static_cast<GLbitfield>(12), cmd.flags);
  EXPECT_EQ(static_cast<GLuint64>(13), cmd.timeout());
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlitFramebufferCHROMIUM) {
  cmds::BlitFramebufferCHROMIUM& cmd =
      *GetBufferAs<cmds::BlitFramebufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLint>(11), static_cast<GLint>(12),
      static_cast<GLint>(13), static_cast<GLint>(14), static_cast<GLint>(15),
      static_cast<GLint>(16), static_cast<GLint>(17), static_cast<GLint>(18),
      static_cast<GLbitfield>(19), static_cast<GLenum>(20));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlitFramebufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.srcX0);
  EXPECT_EQ(static_cast<GLint>(12), cmd.srcY0);
  EXPECT_EQ(static_cast<GLint>(13), cmd.srcX1);
  EXPECT_EQ(static_cast<GLint>(14), cmd.srcY1);
  EXPECT_EQ(static_cast<GLint>(15), cmd.dstX0);
  EXPECT_EQ(static_cast<GLint>(16), cmd.dstY0);
  EXPECT_EQ(static_cast<GLint>(17), cmd.dstX1);
  EXPECT_EQ(static_cast<GLint>(18), cmd.dstY1);
  EXPECT_EQ(static_cast<GLbitfield>(19), cmd.mask);
  EXPECT_EQ(static_cast<GLenum>(20), cmd.filter);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorageMultisampleCHROMIUM) {
  cmds::RenderbufferStorageMultisampleCHROMIUM& cmd =
      *GetBufferAs<cmds::RenderbufferStorageMultisampleCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLsizei>(12), static_cast<GLenum>(13),
                           static_cast<GLsizei>(14), static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::RenderbufferStorageMultisampleCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorageMultisampleAdvancedAMD) {
  cmds::RenderbufferStorageMultisampleAdvancedAMD& cmd =
      *GetBufferAs<cmds::RenderbufferStorageMultisampleAdvancedAMD>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLsizei>(12),
              static_cast<GLsizei>(13), static_cast<GLenum>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::RenderbufferStorageMultisampleAdvancedAMD::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.storageSamples);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorageMultisampleEXT) {
  cmds::RenderbufferStorageMultisampleEXT& cmd =
      *GetBufferAs<cmds::RenderbufferStorageMultisampleEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLsizei>(12), static_cast<GLenum>(13),
                           static_cast<GLsizei>(14), static_cast<GLsizei>(15));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::RenderbufferStorageMultisampleEXT::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTexture2DMultisampleEXT) {
  cmds::FramebufferTexture2DMultisampleEXT& cmd =
      *GetBufferAs<cmds::FramebufferTexture2DMultisampleEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<GLuint>(14),
              static_cast<GLint>(15), static_cast<GLsizei>(16));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::FramebufferTexture2DMultisampleEXT::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.textarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(15), cmd.level);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.samples);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexStorage2DEXT) {
  cmds::TexStorage2DEXT& cmd = *GetBufferAs<cmds::TexStorage2DEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLsizei>(12), static_cast<GLenum>(13),
                           static_cast<GLsizei>(14), static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexStorage2DEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.levels);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalFormat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenQueriesEXTImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::GenQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteQueriesEXTImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::DeleteQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, QueryCounterEXT) {
  cmds::QueryCounterEXT& cmd = *GetBufferAs<cmds::QueryCounterEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<uint32_t>(13),
                           static_cast<uint32_t>(14), static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::QueryCounterEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.id);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.target);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.sync_data_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.submit_count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BeginQueryEXT) {
  cmds::BeginQueryEXT& cmd = *GetBufferAs<cmds::BeginQueryEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.sync_data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BeginTransformFeedback) {
  cmds::BeginTransformFeedback& cmd =
      *GetBufferAs<cmds::BeginTransformFeedback>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginTransformFeedback::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.primitivemode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EndQueryEXT) {
  cmds::EndQueryEXT& cmd = *GetBufferAs<cmds::EndQueryEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::EndQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.submit_count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EndTransformFeedback) {
  cmds::EndTransformFeedback& cmd = *GetBufferAs<cmds::EndTransformFeedback>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::EndTransformFeedback::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SetDisjointValueSyncCHROMIUM) {
  cmds::SetDisjointValueSyncCHROMIUM& cmd =
      *GetBufferAs<cmds::SetDisjointValueSyncCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint32_t>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SetDisjointValueSyncCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.sync_data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, InsertEventMarkerEXT) {
  cmds::InsertEventMarkerEXT& cmd = *GetBufferAs<cmds::InsertEventMarkerEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::InsertEventMarkerEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PushGroupMarkerEXT) {
  cmds::PushGroupMarkerEXT& cmd = *GetBufferAs<cmds::PushGroupMarkerEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::PushGroupMarkerEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PopGroupMarkerEXT) {
  cmds::PopGroupMarkerEXT& cmd = *GetBufferAs<cmds::PopGroupMarkerEXT>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::PopGroupMarkerEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenVertexArraysOESImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenVertexArraysOESImmediate& cmd =
      *GetBufferAs<cmds::GenVertexArraysOESImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenVertexArraysOESImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, DeleteVertexArraysOESImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteVertexArraysOESImmediate& cmd =
      *GetBufferAs<cmds::DeleteVertexArraysOESImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteVertexArraysOESImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(GLES2FormatTest, IsVertexArrayOES) {
  cmds::IsVertexArrayOES& cmd = *GetBufferAs<cmds::IsVertexArrayOES>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsVertexArrayOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.array);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindVertexArrayOES) {
  cmds::BindVertexArrayOES& cmd = *GetBufferAs<cmds::BindVertexArrayOES>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindVertexArrayOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.array);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferParameteri) {
  cmds::FramebufferParameteri& cmd =
      *GetBufferAs<cmds::FramebufferParameteri>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FramebufferParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindImageTexture) {
  cmds::BindImageTexture& cmd = *GetBufferAs<cmds::BindImageTexture>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<GLint>(13),
                           static_cast<GLboolean>(14), static_cast<GLint>(15),
                           static_cast<GLenum>(16), static_cast<GLenum>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindImageTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.unit);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(13), cmd.level);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.layered);
  EXPECT_EQ(static_cast<GLint>(15), cmd.layer);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.access);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DispatchCompute) {
  cmds::DispatchCompute& cmd = *GetBufferAs<cmds::DispatchCompute>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DispatchCompute::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.num_groups_x);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.num_groups_y);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.num_groups_z);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DispatchComputeIndirect) {
  cmds::DispatchComputeIndirect& cmd =
      *GetBufferAs<cmds::DispatchComputeIndirect>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLintptr>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DispatchComputeIndirect::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLintptr>(11), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArraysIndirect) {
  cmds::DrawArraysIndirect& cmd = *GetBufferAs<cmds::DrawArraysIndirect>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawArraysIndirect::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElementsIndirect) {
  cmds::DrawElementsIndirect& cmd = *GetBufferAs<cmds::DrawElementsIndirect>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawElementsIndirect::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInterfaceiv) {
  cmds::GetProgramInterfaceiv& cmd =
      *GetBufferAs<cmds::GetProgramInterfaceiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramInterfaceiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.program_interface);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramResourceIndex) {
  cmds::GetProgramResourceIndex& cmd =
      *GetBufferAs<cmds::GetProgramResourceIndex>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramResourceIndex::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.program_interface);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.index_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.index_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramResourceName) {
  cmds::GetProgramResourceName& cmd =
      *GetBufferAs<cmds::GetProgramResourceName>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<GLuint>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramResourceName::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.program_interface);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramResourceiv) {
  cmds::GetProgramResourceiv& cmd = *GetBufferAs<cmds::GetProgramResourceiv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<GLuint>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramResourceiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.program_interface);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.props_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramResourceLocation) {
  cmds::GetProgramResourceLocation& cmd =
      *GetBufferAs<cmds::GetProgramResourceLocation>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramResourceLocation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.program_interface);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.location_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.location_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MemoryBarrierEXT) {
  cmds::MemoryBarrierEXT& cmd = *GetBufferAs<cmds::MemoryBarrierEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLbitfield>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MemoryBarrierEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLbitfield>(11), cmd.barriers);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MemoryBarrierByRegion) {
  cmds::MemoryBarrierByRegion& cmd =
      *GetBufferAs<cmds::MemoryBarrierByRegion>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLbitfield>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MemoryBarrierByRegion::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLbitfield>(11), cmd.barriers);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SwapBuffers) {
  cmds::SwapBuffers& cmd = *GetBufferAs<cmds::SwapBuffers>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint64>(11), static_cast<GLbitfield>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SwapBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint64>(11), cmd.swap_id());
  EXPECT_EQ(static_cast<GLbitfield>(12), cmd.flags);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetMaxValueInBufferCHROMIUM) {
  cmds::GetMaxValueInBufferCHROMIUM& cmd =
      *GetBufferAs<cmds::GetMaxValueInBufferCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLsizei>(12),
              static_cast<GLenum>(13), static_cast<GLuint>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetMaxValueInBufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.offset);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableFeatureCHROMIUM) {
  cmds::EnableFeatureCHROMIUM& cmd =
      *GetBufferAs<cmds::EnableFeatureCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::EnableFeatureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MapBufferRange) {
  cmds::MapBufferRange& cmd = *GetBufferAs<cmds::MapBufferRange>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLintptr>(12),
              static_cast<GLsizeiptr>(13), static_cast<GLbitfield>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16),
              static_cast<uint32_t>(17), static_cast<uint32_t>(18));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MapBufferRange::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLintptr>(12), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.size);
  EXPECT_EQ(static_cast<GLbitfield>(14), cmd.access);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UnmapBuffer) {
  cmds::UnmapBuffer& cmd = *GetBufferAs<cmds::UnmapBuffer>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::UnmapBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FlushMappedBufferRange) {
  cmds::FlushMappedBufferRange& cmd =
      *GetBufferAs<cmds::FlushMappedBufferRange>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLintptr>(12),
              static_cast<GLsizeiptr>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FlushMappedBufferRange::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLintptr>(12), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ResizeCHROMIUM) {
  cmds::ResizeCHROMIUM& cmd = *GetBufferAs<cmds::ResizeCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLfloat>(13), static_cast<GLboolean>(14),
                           static_cast<GLuint>(15), static_cast<GLuint>(16),
                           static_cast<GLsizei>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ResizeCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.width);
  EXPECT_EQ(static_cast<GLint>(12), cmd.height);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.scale_factor);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.alpha);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.color_space_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetRequestableExtensionsCHROMIUM) {
  cmds::GetRequestableExtensionsCHROMIUM& cmd =
      *GetBufferAs<cmds::GetRequestableExtensionsCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<uint32_t>(11));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::GetRequestableExtensionsCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RequestExtensionCHROMIUM) {
  cmds::RequestExtensionCHROMIUM& cmd =
      *GetBufferAs<cmds::RequestExtensionCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<uint32_t>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::RequestExtensionCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInfoCHROMIUM) {
  cmds::GetProgramInfoCHROMIUM& cmd =
      *GetBufferAs<cmds::GetProgramInfoCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetProgramInfoCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformBlocksCHROMIUM) {
  cmds::GetUniformBlocksCHROMIUM& cmd =
      *GetBufferAs<cmds::GetUniformBlocksCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformBlocksCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTransformFeedbackVaryingsCHROMIUM) {
  cmds::GetTransformFeedbackVaryingsCHROMIUM& cmd =
      *GetBufferAs<cmds::GetTransformFeedbackVaryingsCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::GetTransformFeedbackVaryingsCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformsES3CHROMIUM) {
  cmds::GetUniformsES3CHROMIUM& cmd =
      *GetBufferAs<cmds::GetUniformsES3CHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetUniformsES3CHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DescheduleUntilFinishedCHROMIUM) {
  cmds::DescheduleUntilFinishedCHROMIUM& cmd =
      *GetBufferAs<cmds::DescheduleUntilFinishedCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::DescheduleUntilFinishedCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTranslatedShaderSourceANGLE) {
  cmds::GetTranslatedShaderSourceANGLE& cmd =
      *GetBufferAs<cmds::GetTranslatedShaderSourceANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetTranslatedShaderSourceANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTextureCHROMIUM) {
  cmds::CopyTextureCHROMIUM& cmd = *GetBufferAs<cmds::CopyTextureCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLuint>(14),
              static_cast<GLint>(15), static_cast<GLint>(16),
              static_cast<GLenum>(17), static_cast<GLboolean>(18),
              static_cast<GLboolean>(19), static_cast<GLboolean>(20));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopyTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.source_id);
  EXPECT_EQ(static_cast<GLint>(12), cmd.source_level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.dest_target);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.dest_id);
  EXPECT_EQ(static_cast<GLint>(15), cmd.dest_level);
  EXPECT_EQ(static_cast<GLint>(16), cmd.internalformat);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.dest_type);
  EXPECT_EQ(static_cast<GLboolean>(18), cmd.unpack_flip_y);
  EXPECT_EQ(static_cast<GLboolean>(19), cmd.unpack_premultiply_alpha);
  EXPECT_EQ(static_cast<GLboolean>(20), cmd.unpack_unmultiply_alpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopySubTextureCHROMIUM) {
  cmds::CopySubTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::CopySubTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
      static_cast<GLenum>(13), static_cast<GLuint>(14), static_cast<GLint>(15),
      static_cast<GLint>(16), static_cast<GLint>(17), static_cast<GLint>(18),
      static_cast<GLint>(19), static_cast<GLsizei>(20),
      static_cast<GLsizei>(21), static_cast<GLboolean>(22),
      static_cast<GLboolean>(23), static_cast<GLboolean>(24));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopySubTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.source_id);
  EXPECT_EQ(static_cast<GLint>(12), cmd.source_level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.dest_target);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.dest_id);
  EXPECT_EQ(static_cast<GLint>(15), cmd.dest_level);
  EXPECT_EQ(static_cast<GLint>(16), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(17), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(18), cmd.x);
  EXPECT_EQ(static_cast<GLint>(19), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(20), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(21), cmd.height);
  EXPECT_EQ(static_cast<GLboolean>(22), cmd.unpack_flip_y);
  EXPECT_EQ(static_cast<GLboolean>(23), cmd.unpack_premultiply_alpha);
  EXPECT_EQ(static_cast<GLboolean>(24), cmd.unpack_unmultiply_alpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArraysInstancedANGLE) {
  cmds::DrawArraysInstancedANGLE& cmd =
      *GetBufferAs<cmds::DrawArraysInstancedANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLsizei>(13), static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawArraysInstancedANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.primcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArraysInstancedBaseInstanceANGLE) {
  cmds::DrawArraysInstancedBaseInstanceANGLE& cmd =
      *GetBufferAs<cmds::DrawArraysInstancedBaseInstanceANGLE>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLint>(12), static_cast<GLsizei>(13),
                           static_cast<GLsizei>(14), static_cast<GLuint>(15));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::DrawArraysInstancedBaseInstanceANGLE::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.primcount);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.baseinstance);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElementsInstancedANGLE) {
  cmds::DrawElementsInstancedANGLE& cmd =
      *GetBufferAs<cmds::DrawElementsInstancedANGLE>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLsizei>(12), static_cast<GLenum>(13),
                           static_cast<GLuint>(14), static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawElementsInstancedANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.primcount);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElementsInstancedBaseVertexBaseInstanceANGLE) {
  cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE& cmd =
      *GetBufferAs<cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLsizei>(12), static_cast<GLenum>(13),
                           static_cast<GLuint>(14), static_cast<GLsizei>(15),
                           static_cast<GLint>(16), static_cast<GLuint>(17));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.primcount);
  EXPECT_EQ(static_cast<GLint>(16), cmd.basevertex);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.baseinstance);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttribDivisorANGLE) {
  cmds::VertexAttribDivisorANGLE& cmd =
      *GetBufferAs<cmds::VertexAttribDivisorANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::VertexAttribDivisorANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.divisor);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ProduceTextureDirectCHROMIUMImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::ProduceTextureDirectCHROMIUMImmediate& cmd =
      *GetBufferAs<cmds::ProduceTextureDirectCHROMIUMImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ProduceTextureDirectCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, CreateAndConsumeTextureINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::CreateAndConsumeTextureINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CreateAndConsumeTextureINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::CreateAndConsumeTextureINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, BindUniformLocationCHROMIUMBucket) {
  cmds::BindUniformLocationCHROMIUMBucket& cmd =
      *GetBufferAs<cmds::BindUniformLocationCHROMIUMBucket>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLint>(12), static_cast<uint32_t>(13));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::BindUniformLocationCHROMIUMBucket::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TraceBeginCHROMIUM) {
  cmds::TraceBeginCHROMIUM& cmd = *GetBufferAs<cmds::TraceBeginCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TraceBeginCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.category_bucket_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TraceEndCHROMIUM) {
  cmds::TraceEndCHROMIUM& cmd = *GetBufferAs<cmds::TraceEndCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::TraceEndCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DiscardFramebufferEXTImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
      static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
      static_cast<GLenum>(kSomeBaseValueToTestWith + 1),
  };
  cmds::DiscardFramebufferEXTImmediate& cmd =
      *GetBufferAs<cmds::DiscardFramebufferEXTImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DiscardFramebufferEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(1), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, LoseContextCHROMIUM) {
  cmds::LoseContextCHROMIUM& cmd = *GetBufferAs<cmds::LoseContextCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LoseContextCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.current);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.other);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawBuffersEXTImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
      static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
  };
  cmds::DrawBuffersEXTImmediate& cmd =
      *GetBufferAs<cmds::DrawBuffersEXTImmediate>();
  const GLsizei kNumElements = 1;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(1), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DrawBuffersEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(1), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, DiscardBackbufferCHROMIUM) {
  cmds::DiscardBackbufferCHROMIUM& cmd =
      *GetBufferAs<cmds::DiscardBackbufferCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DiscardBackbufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FlushDriverCachesCHROMIUM) {
  cmds::FlushDriverCachesCHROMIUM& cmd =
      *GetBufferAs<cmds::FlushDriverCachesCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::FlushDriverCachesCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SetActiveURLCHROMIUM) {
  cmds::SetActiveURLCHROMIUM& cmd = *GetBufferAs<cmds::SetActiveURLCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SetActiveURLCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.url_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ContextVisibilityHintCHROMIUM) {
  cmds::ContextVisibilityHintCHROMIUM& cmd =
      *GetBufferAs<cmds::ContextVisibilityHintCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLboolean>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ContextVisibilityHintCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.visibility);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendBarrierKHR) {
  cmds::BlendBarrierKHR& cmd = *GetBufferAs<cmds::BlendBarrierKHR>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendBarrierKHR::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindFragDataLocationIndexedEXTBucket) {
  cmds::BindFragDataLocationIndexedEXTBucket& cmd =
      *GetBufferAs<cmds::BindFragDataLocationIndexedEXTBucket>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::BindFragDataLocationIndexedEXTBucket::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.colorNumber);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindFragDataLocationEXTBucket) {
  cmds::BindFragDataLocationEXTBucket& cmd =
      *GetBufferAs<cmds::BindFragDataLocationEXTBucket>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindFragDataLocationEXTBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.colorNumber);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFragDataIndexEXT) {
  cmds::GetFragDataIndexEXT& cmd = *GetBufferAs<cmds::GetFragDataIndexEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetFragDataIndexEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.index_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.index_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, InitializeDiscardableTextureCHROMIUM) {
  cmds::InitializeDiscardableTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::InitializeDiscardableTextureCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::InitializeDiscardableTextureCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UnlockDiscardableTextureCHROMIUM) {
  cmds::UnlockDiscardableTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::UnlockDiscardableTextureCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::UnlockDiscardableTextureCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LockDiscardableTextureCHROMIUM) {
  cmds::LockDiscardableTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::LockDiscardableTextureCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LockDiscardableTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, WindowRectanglesEXTImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
      static_cast<GLint>(kSomeBaseValueToTestWith + 4),
      static_cast<GLint>(kSomeBaseValueToTestWith + 5),
      static_cast<GLint>(kSomeBaseValueToTestWith + 6),
      static_cast<GLint>(kSomeBaseValueToTestWith + 7),
  };
  cmds::WindowRectanglesEXTImmediate& cmd =
      *GetBufferAs<cmds::WindowRectanglesEXTImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 4;
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(1), static_cast<GLsizei>(2), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::WindowRectanglesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(1), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, CreateGpuFenceINTERNAL) {
  cmds::CreateGpuFenceINTERNAL& cmd =
      *GetBufferAs<cmds::CreateGpuFenceINTERNAL>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CreateGpuFenceINTERNAL::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.gpu_fence_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, WaitGpuFenceCHROMIUM) {
  cmds::WaitGpuFenceCHROMIUM& cmd = *GetBufferAs<cmds::WaitGpuFenceCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::WaitGpuFenceCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.gpu_fence_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DestroyGpuFenceCHROMIUM) {
  cmds::DestroyGpuFenceCHROMIUM& cmd =
      *GetBufferAs<cmds::DestroyGpuFenceCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DestroyGpuFenceCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.gpu_fence_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SetReadbackBufferShadowAllocationINTERNAL) {
  cmds::SetReadbackBufferShadowAllocationINTERNAL& cmd =
      *GetBufferAs<cmds::SetReadbackBufferShadowAllocationINTERNAL>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
              static_cast<GLuint>(13), static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::SetReadbackBufferShadowAllocationINTERNAL::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer_id);
  EXPECT_EQ(static_cast<GLint>(12), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTextureMultiviewOVR) {
  cmds::FramebufferTextureMultiviewOVR& cmd =
      *GetBufferAs<cmds::FramebufferTextureMultiviewOVR>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLuint>(13), static_cast<GLint>(14),
              static_cast<GLint>(15), static_cast<GLsizei>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::FramebufferTextureMultiviewOVR::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(14), cmd.level);
  EXPECT_EQ(static_cast<GLint>(15), cmd.baseViewIndex);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.numViews);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, MaxShaderCompilerThreadsKHR) {
  cmds::MaxShaderCompilerThreadsKHR& cmd =
      *GetBufferAs<cmds::MaxShaderCompilerThreadsKHR>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::MaxShaderCompilerThreadsKHR::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateAndTexStorage2DSharedImageINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(
      static_cast<uint32_t>(
          cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, BeginSharedImageAccessDirectCHROMIUM) {
  cmds::BeginSharedImageAccessDirectCHROMIUM& cmd =
      *GetBufferAs<cmds::BeginSharedImageAccessDirectCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::BeginSharedImageAccessDirectCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EndSharedImageAccessDirectCHROMIUM) {
  cmds::EndSharedImageAccessDirectCHROMIUM& cmd =
      *GetBufferAs<cmds::EndSharedImageAccessDirectCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::EndSharedImageAccessDirectCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ConvertRGBAToYUVAMailboxesINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 64),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 65),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 66),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 67),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 68),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 69),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 70),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 71),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 72),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 73),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 74),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 75),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 76),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 77),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 78),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 79),
  };
  cmds::ConvertRGBAToYUVAMailboxesINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ConvertRGBAToYUVAMailboxesINTERNALImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ConvertRGBAToYUVAMailboxesINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.planes_yuv_color_space);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.plane_config);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.subsampling);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, ConvertYUVAMailboxesToRGBINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 64),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 65),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 66),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 67),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 68),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 69),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 70),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 71),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 72),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 73),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 74),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 75),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 76),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 77),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 78),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 79),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 80),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 81),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 82),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 83),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 84),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 85),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 86),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 87),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 88),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 89),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 90),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 91),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 92),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 93),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 94),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 95),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 96),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 97),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 98),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 99),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 100),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 101),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 102),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 103),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 104),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 105),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 106),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 107),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 108),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 109),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 110),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 111),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 112),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 113),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 114),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 115),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 116),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 117),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 118),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 119),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 120),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 121),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 122),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 123),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 124),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 125),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 126),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 127),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 128),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 129),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 130),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 131),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 132),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 133),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 134),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 135),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 136),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 137),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 138),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 139),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 140),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 141),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 142),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 143),
  };
  cmds::ConvertYUVAMailboxesToRGBINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ConvertYUVAMailboxesToRGBINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLsizei>(13), static_cast<GLsizei>(14),
                           static_cast<GLenum>(15), static_cast<GLenum>(16),
                           static_cast<GLenum>(17), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ConvertYUVAMailboxesToRGBINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.src_x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.src_y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.planes_yuv_color_space);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.plane_config);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.subsampling);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, ConvertYUVAMailboxesToTextureINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
  };
  cmds::ConvertYUVAMailboxesToTextureINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ConvertYUVAMailboxesToTextureINTERNALImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
              static_cast<GLuint>(13), static_cast<GLenum>(14),
              static_cast<GLint>(15), static_cast<GLint>(16),
              static_cast<GLsizei>(17), static_cast<GLsizei>(18),
              static_cast<GLboolean>(19), static_cast<GLenum>(20),
              static_cast<GLenum>(21), static_cast<GLenum>(22), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ConvertYUVAMailboxesToTextureINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.internal_format);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.type);
  EXPECT_EQ(static_cast<GLint>(15), cmd.src_x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.src_y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
  EXPECT_EQ(static_cast<GLboolean>(19), cmd.flip_y);
  EXPECT_EQ(static_cast<GLenum>(20), cmd.planes_yuv_color_space);
  EXPECT_EQ(static_cast<GLenum>(21), cmd.plane_config);
  EXPECT_EQ(static_cast<GLenum>(22), cmd.subsampling);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, CopySharedImageINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
  };
  cmds::CopySharedImageINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CopySharedImageINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLint>(13), static_cast<GLint>(14),
                           static_cast<GLsizei>(15), static_cast<GLsizei>(16),
                           static_cast<GLboolean>(17), data);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::CopySharedImageINTERNALImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(12), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(13), cmd.x);
  EXPECT_EQ(static_cast<GLint>(14), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLboolean>(17), cmd.unpack_flip_y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, CopySharedImageToTextureINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::CopySharedImageToTextureINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CopySharedImageToTextureINTERNALImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLuint>(11), static_cast<GLenum>(12),
      static_cast<GLuint>(13), static_cast<GLenum>(14), static_cast<GLint>(15),
      static_cast<GLint>(16), static_cast<GLsizei>(17),
      static_cast<GLsizei>(18), static_cast<GLboolean>(19), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::CopySharedImageToTextureINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.internal_format);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.type);
  EXPECT_EQ(static_cast<GLint>(15), cmd.src_x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.src_y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
  EXPECT_EQ(static_cast<GLboolean>(19), cmd.flip_y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, ReadbackARGBImagePixelsINTERNAL) {
  cmds::ReadbackARGBImagePixelsINTERNAL& cmd =
      *GetBufferAs<cmds::ReadbackARGBImagePixelsINTERNAL>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLint>(11), static_cast<GLint>(12),
      static_cast<GLint>(13), static_cast<GLuint>(14), static_cast<GLuint>(15),
      static_cast<GLuint>(16), static_cast<GLuint>(17), static_cast<GLuint>(18),
      static_cast<GLint>(19), static_cast<GLuint>(20), static_cast<GLuint>(21),
      static_cast<GLuint>(22), static_cast<GLuint>(23));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::ReadbackARGBImagePixelsINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.src_x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.src_y);
  EXPECT_EQ(static_cast<GLint>(13), cmd.plane_index);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.dst_width);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.dst_height);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.row_bytes);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.dst_sk_color_type);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.dst_sk_alpha_type);
  EXPECT_EQ(static_cast<GLint>(19), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(20), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLuint>(21), cmd.color_space_offset);
  EXPECT_EQ(static_cast<GLuint>(22), cmd.pixels_offset);
  EXPECT_EQ(static_cast<GLuint>(23), cmd.mailbox_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, WritePixelsYUVINTERNAL) {
  cmds::WritePixelsYUVINTERNAL& cmd =
      *GetBufferAs<cmds::WritePixelsYUVINTERNAL>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
      static_cast<GLuint>(13), static_cast<GLuint>(14), static_cast<GLuint>(15),
      static_cast<GLuint>(16), static_cast<GLuint>(17), static_cast<GLuint>(18),
      static_cast<GLuint>(19), static_cast<GLint>(20), static_cast<GLuint>(21),
      static_cast<GLuint>(22), static_cast<GLuint>(23), static_cast<GLuint>(24),
      static_cast<GLuint>(25));
  EXPECT_EQ(static_cast<uint32_t>(cmds::WritePixelsYUVINTERNAL::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.src_width);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.src_height);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.src_row_bytes_plane1);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.src_row_bytes_plane2);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.src_row_bytes_plane3);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.src_row_bytes_plane4);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.src_yuv_plane_config);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.src_yuv_subsampling);
  EXPECT_EQ(static_cast<GLuint>(19), cmd.src_yuv_datatype);
  EXPECT_EQ(static_cast<GLint>(20), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(21), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLuint>(22), cmd.pixels_offset_plane1);
  EXPECT_EQ(static_cast<GLuint>(23), cmd.pixels_offset_plane2);
  EXPECT_EQ(static_cast<GLuint>(24), cmd.pixels_offset_plane3);
  EXPECT_EQ(static_cast<GLuint>(25), cmd.pixels_offset_plane4);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableiOES) {
  cmds::EnableiOES& cmd = *GetBufferAs<cmds::EnableiOES>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::EnableiOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DisableiOES) {
  cmds::DisableiOES& cmd = *GetBufferAs<cmds::DisableiOES>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DisableiOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquationiOES) {
  cmds::BlendEquationiOES& cmd = *GetBufferAs<cmds::BlendEquationiOES>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendEquationiOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buf);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquationSeparateiOES) {
  cmds::BlendEquationSeparateiOES& cmd =
      *GetBufferAs<cmds::BlendEquationSeparateiOES>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<GLenum>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendEquationSeparateiOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buf);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.modeRGB);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.modeAlpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFunciOES) {
  cmds::BlendFunciOES& cmd = *GetBufferAs<cmds::BlendFunciOES>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<GLenum>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendFunciOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buf);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.src);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.dst);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFuncSeparateiOES) {
  cmds::BlendFuncSeparateiOES& cmd =
      *GetBufferAs<cmds::BlendFuncSeparateiOES>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<GLenum>(13),
                           static_cast<GLenum>(14), static_cast<GLenum>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BlendFuncSeparateiOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buf);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.srcRGB);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.dstRGB);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.srcAlpha);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.dstAlpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ColorMaskiOES) {
  cmds::ColorMaskiOES& cmd = *GetBufferAs<cmds::ColorMaskiOES>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLboolean>(12),
              static_cast<GLboolean>(13), static_cast<GLboolean>(14),
              static_cast<GLboolean>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ColorMaskiOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buf);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.r);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.g);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.b);
  EXPECT_EQ(static_cast<GLboolean>(15), cmd.a);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsEnablediOES) {
  cmds::IsEnablediOES& cmd = *GetBufferAs<cmds::IsEnablediOES>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::IsEnablediOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ProvokingVertexANGLE) {
  cmds::ProvokingVertexANGLE& cmd = *GetBufferAs<cmds::ProvokingVertexANGLE>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ProvokingVertexANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.provokeMode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferMemorylessPixelLocalStorageANGLE) {
  cmds::FramebufferMemorylessPixelLocalStorageANGLE& cmd =
      *GetBufferAs<cmds::FramebufferMemorylessPixelLocalStorageANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferMemorylessPixelLocalStorageANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.internalformat);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTexturePixelLocalStorageANGLE) {
  cmds::FramebufferTexturePixelLocalStorageANGLE& cmd =
      *GetBufferAs<cmds::FramebufferTexturePixelLocalStorageANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLuint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferTexturePixelLocalStorageANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.backingtexture);
  EXPECT_EQ(static_cast<GLint>(13), cmd.level);
  EXPECT_EQ(static_cast<GLint>(14), cmd.layer);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferPixelLocalClearValuefvANGLEImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
      static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  cmds::FramebufferPixelLocalClearValuefvANGLEImmediate& cmd =
      *GetBufferAs<cmds::FramebufferPixelLocalClearValuefvANGLEImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferPixelLocalClearValuefvANGLEImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, FramebufferPixelLocalClearValueivANGLEImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
      static_cast<GLint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::FramebufferPixelLocalClearValueivANGLEImmediate& cmd =
      *GetBufferAs<cmds::FramebufferPixelLocalClearValueivANGLEImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferPixelLocalClearValueivANGLEImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, FramebufferPixelLocalClearValueuivANGLEImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::FramebufferPixelLocalClearValueuivANGLEImmediate& cmd =
      *GetBufferAs<cmds::FramebufferPixelLocalClearValueuivANGLEImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferPixelLocalClearValueuivANGLEImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, BeginPixelLocalStorageANGLEImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
      static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
  };
  cmds::BeginPixelLocalStorageANGLEImmediate& cmd =
      *GetBufferAs<cmds::BeginPixelLocalStorageANGLEImmediate>();
  const GLsizei kNumElements = 1;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(1), data);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::BeginPixelLocalStorageANGLEImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(1), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, EndPixelLocalStorageANGLEImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
      static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
  };
  cmds::EndPixelLocalStorageANGLEImmediate& cmd =
      *GetBufferAs<cmds::EndPixelLocalStorageANGLEImmediate>();
  const GLsizei kNumElements = 1;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(1), data);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::EndPixelLocalStorageANGLEImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(1), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(GLES2FormatTest, PixelLocalStorageBarrierANGLE) {
  cmds::PixelLocalStorageBarrierANGLE& cmd =
      *GetBufferAs<cmds::PixelLocalStorageBarrierANGLE>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::PixelLocalStorageBarrierANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferPixelLocalStorageInterruptANGLE) {
  cmds::FramebufferPixelLocalStorageInterruptANGLE& cmd =
      *GetBufferAs<cmds::FramebufferPixelLocalStorageInterruptANGLE>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferPixelLocalStorageInterruptANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferPixelLocalStorageRestoreANGLE) {
  cmds::FramebufferPixelLocalStorageRestoreANGLE& cmd =
      *GetBufferAs<cmds::FramebufferPixelLocalStorageRestoreANGLE>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::FramebufferPixelLocalStorageRestoreANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFramebufferPixelLocalStorageParameterfvANGLE) {
  cmds::GetFramebufferPixelLocalStorageParameterfvANGLE& cmd =
      *GetBufferAs<cmds::GetFramebufferPixelLocalStorageParameterfvANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::GetFramebufferPixelLocalStorageParameterfvANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFramebufferPixelLocalStorageParameterivANGLE) {
  cmds::GetFramebufferPixelLocalStorageParameterivANGLE& cmd =
      *GetBufferAs<cmds::GetFramebufferPixelLocalStorageParameterivANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLenum>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::GetFramebufferPixelLocalStorageParameterivANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.plane);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClipControlEXT) {
  cmds::ClipControlEXT& cmd = *GetBufferAs<cmds::ClipControlEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClipControlEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.origin);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.depth);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PolygonModeANGLE) {
  cmds::PolygonModeANGLE& cmd = *GetBufferAs<cmds::PolygonModeANGLE>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::PolygonModeANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PolygonOffsetClampEXT) {
  cmds::PolygonOffsetClampEXT& cmd =
      *GetBufferAs<cmds::PolygonOffsetClampEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLfloat>(11),
                           static_cast<GLfloat>(12), static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::PolygonOffsetClampEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.factor);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.units);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.clamp);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_
