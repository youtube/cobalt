// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_H_

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {
namespace gles2 {

class GLES2DecoderTest : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest() = default;

 protected:
  void CheckReadPixelsOutOfRange(GLint in_read_x,
                                 GLint in_read_y,
                                 GLsizei in_read_width,
                                 GLsizei in_read_height,
                                 bool init);
};

class GLES2DecoderWithShaderTest : public GLES2DecoderWithShaderTestBase {
 public:
  GLES2DecoderWithShaderTest() : GLES2DecoderWithShaderTestBase() {}

  void CheckTextureChangesMarkFBOAsNotComplete(bool bound_fbo);
  void CheckRenderbufferChangesMarkFBOAsNotComplete(bool bound_fbo);
};

class GLES2DecoderRGBBackbufferTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderRGBBackbufferTest() = default;

  void SetUp() override;
};

class GLES2DecoderManualInitTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderManualInitTest() = default;

  // Override default setup so nothing gets setup.
  void SetUp() override {}

  void DirtyStateMaskTest(GLuint color_bits,
                          bool depth_mask,
                          GLuint front_stencil_mask,
                          GLuint back_stencil_mask);
  void EnableDisableTest(GLenum cap, bool enable, bool expect_set);
};

class GLES3DecoderTest : public GLES2DecoderTest {
 public:
  GLES3DecoderTest() { shader_language_version_ = 300; }

  // Override default setup so ES3 capabilities are enabled by default.
  void SetUp() override;
};

class WebGL2DecoderTest : public GLES2DecoderTest {
 public:
  WebGL2DecoderTest() { shader_language_version_ = 300; }

  // Override default setup so ES3 capabilities are enabled by default
  // and WebGL2 specific rules are enforced.
  void SetUp() override;
};

class GLES3DecoderWithShaderTest : public GLES2DecoderWithShaderTest {
 public:
  GLES3DecoderWithShaderTest() { shader_language_version_ = 300; }

  // Override default setup so ES3 capabilities are enabled by default.
  void SetUp() override;
};

class GLES3DecoderManualInitTest : public GLES2DecoderManualInitTest {
 public:
  GLES3DecoderManualInitTest() { shader_language_version_ = 300; }
};

class GLES2DecoderPassthroughTest : public GLES2DecoderPassthroughTestBase {
 public:
  GLES2DecoderPassthroughTest()
      : GLES2DecoderPassthroughTestBase(CONTEXT_TYPE_OPENGLES2) {}
};

class GLES2WebGLDecoderPassthroughTest
    : public GLES2DecoderPassthroughTestBase {
 public:
  GLES2WebGLDecoderPassthroughTest()
      : GLES2DecoderPassthroughTestBase(CONTEXT_TYPE_WEBGL1) {}
};

class GLES3DecoderPassthroughTest : public GLES2DecoderPassthroughTestBase {
 public:
  GLES3DecoderPassthroughTest()
      : GLES2DecoderPassthroughTestBase(CONTEXT_TYPE_OPENGLES3) {}
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_H_
