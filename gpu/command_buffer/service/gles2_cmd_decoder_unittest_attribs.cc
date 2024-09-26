// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

TEST_P(GLES2DecoderTest, DisableVertexAttribArrayValidArgs) {
  SetDriverVertexAttribEnabled(1, false);
  SpecializedSetup<cmds::DisableVertexAttribArray, 0>(true);
  cmds::DisableVertexAttribArray cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, EnableVertexAttribArrayValidArgs) {
  SetDriverVertexAttribEnabled(1, true);
  SpecializedSetup<cmds::EnableVertexAttribArray, 0>(true);
  cmds::EnableVertexAttribArray cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, EnabledVertexAttribArrayIsDisabledIfUnused) {
  SetupExpectationsForApplyingDefaultDirtyState();

  // Set up and enable attribs 0, 1, 2
  SetupAllNeededVertexBuffers();
  // Enable attrib 3, and verify it's called in the driver
  {
    EXPECT_CALL(*gl_, EnableVertexAttribArray(3))
        .Times(1)
        .RetiresOnSaturation();
    cmds::EnableVertexAttribArray cmd;
    cmd.Init(3);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  DoVertexAttribPointer(3, 2, GL_FLOAT, 0, 0);

  // Expect the draw call below causes attrib 3 to be disabled in the driver
  EXPECT_CALL(*gl_, DisableVertexAttribArray(3)).Times(1).RetiresOnSaturation();
  // Perform a draw which uses only attributes 0, 1, 2 - not attrib 3
  {
    EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
        .Times(1)
        .RetiresOnSaturation();
    cmds::DrawArrays cmd;
    cmd.Init(GL_TRIANGLES, 0, kNumVertices);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
}

TEST_P(GLES2DecoderWithShaderTest, GetVertexAttribPointervSucceeds) {
  const GLuint kOffsetToTestFor = sizeof(float) * 4;
  const GLuint kIndexToTest = 1;
  auto* result = static_cast<cmds::GetVertexAttribPointerv::Result*>(
      shared_memory_address_);
  result->size = 0;
  const GLuint* result_value = result->GetData();
  // Test that initial value is 0.
  cmds::GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest,
           GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(0u, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Set the value and see that we get it.
  SetupVertexBuffer();
  DoVertexAttribPointer(kIndexToTest, 2, GL_FLOAT, 0, kOffsetToTestFor);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(kOffsetToTestFor, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetVertexAttribPointervBadArgsFails) {
  const GLuint kIndexToTest = 1;
  auto* result = static_cast<cmds::GetVertexAttribPointerv::Result*>(
      shared_memory_address_);
  result->size = 0;
  const GLuint* result_value = result->GetData();
  // Test pname invalid fails.
  cmds::GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest,
           GL_VERTEX_ATTRIB_ARRAY_POINTER + 1,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Test index out of range fails.
  result->size = 0;
  cmd.Init(kNumVertexAttribs,
           GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Test memory id bad fails.
  cmd.Init(kIndexToTest,
           GL_VERTEX_ATTRIB_ARRAY_POINTER,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Test memory offset bad fails.
  cmd.Init(kIndexToTest,
           GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, BindBufferToDifferentTargetFails) {
  // Bind the buffer to GL_ARRAY_BUFFER
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  // Attempt to rebind to GL_ELEMENT_ARRAY_BUFFER
  // NOTE: Real GLES2 does not have this restriction but WebGL and we do.
  // This can be restriction can be removed at runtime.
  EXPECT_CALL(*gl_, BindBuffer(_, _)).Times(0);
  cmds::BindBuffer cmd;
  cmd.Init(GL_ELEMENT_ARRAY_BUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, VertexAttribPointer) {
  SetupVertexBuffer();
  static const GLenum types[] = {
      GL_BYTE,  GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT,
      GL_FLOAT, GL_FIXED,         GL_INT,   GL_UNSIGNED_INT,
  };
  static const GLsizei sizes[] = {
      1, 1, 2, 2, 4, 4, 4, 4,
  };
  static const GLuint indices[] = {
      0, 1, kNumVertexAttribs - 1, kNumVertexAttribs,
  };
  static const GLsizei offset_mult[] = {
      0, 0, 1, 1, 2, 1000,
  };
  static const GLsizei offset_offset[] = {
      0, 1, 0, 1, 0, 0,
  };
  static const GLsizei stride_mult[] = {
      -1, 0, 0, 1, 1, 2, 1000,
  };
  static const GLsizei stride_offset[] = {
      0, 0, 1, 0, 1, 0, 0,
  };
  for (size_t tt = 0; tt < std::size(types); ++tt) {
    GLenum type = types[tt];
    GLsizei num_bytes = sizes[tt];
    for (size_t ii = 0; ii < std::size(indices); ++ii) {
      GLuint index = indices[ii];
      for (GLint size = 0; size < 5; ++size) {
        for (size_t oo = 0; oo < std::size(offset_mult); ++oo) {
          GLuint offset = num_bytes * offset_mult[oo] + offset_offset[oo];
          for (size_t ss = 0; ss < std::size(stride_mult); ++ss) {
            GLsizei stride = num_bytes * stride_mult[ss] + stride_offset[ss];
            for (int normalize = 0; normalize < 2; ++normalize) {
              bool index_good = index < static_cast<GLuint>(kNumVertexAttribs);
              bool size_good = (size > 0 && size < 5);
              bool offset_good = (offset % num_bytes == 0);
              bool stride_good =
                  (stride % num_bytes == 0) && stride >= 0 && stride <= 255;
              bool type_good = (type != GL_INT && type != GL_UNSIGNED_INT &&
                                type != GL_FIXED);
              bool good = size_good && offset_good && stride_good &&
                          type_good && index_good;
              bool call = good && (type != GL_FIXED);
              if (call) {
                EXPECT_CALL(*gl_,
                            VertexAttribPointer(index,
                                                size,
                                                type,
                                                normalize,
                                                stride,
                                                BufferOffset(offset)));
              }
              cmds::VertexAttribPointer cmd;
              cmd.Init(index, size, type, normalize, stride, offset);
              EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
              if (good) {
                EXPECT_EQ(GL_NO_ERROR, GetGLError());
              } else if (size_good && offset_good && stride_good && type_good &&
                         !index_good) {
                EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
              } else if (size_good && offset_good && stride_good &&
                         !type_good && index_good) {
                EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
              } else if (size_good && offset_good && !stride_good &&
                         type_good && index_good) {
                if (stride < 0 || stride > 255) {
                  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
                } else {
                  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
                }
              } else if (size_good && !offset_good && stride_good &&
                         type_good && index_good) {
                EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
              } else if (!size_good && offset_good && stride_good &&
                         type_good && index_good) {
                EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
              } else {
                EXPECT_NE(GL_NO_ERROR, GetGLError());
              }
            }
          }
        }
      }
    }
  }
}

class GLES2DecoderVertexArraysOESTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderVertexArraysOESTest() = default;

  bool vertex_array_deleted_manually_;

  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 2.0";
    init.bind_generates_resource = true;
    InitDecoder(init);
    SetupDefaultProgram();

    AddExpectationsForGenVertexArraysOES();
    GenHelper<cmds::GenVertexArraysOESImmediate>(client_vertexarray_id_);

    vertex_array_deleted_manually_ = false;
  }

  void TearDown() override {
    // This should only be set if the test handled deletion of the vertex array
    // itself. Necessary because vertex_array_objects are not sharable, and thus
    // not managed in the ContextGroup, meaning they will be destroyed during
    // test tear down
    if (!vertex_array_deleted_manually_) {
      AddExpectationsForDeleteVertexArraysOES();
    }

    GLES2DecoderWithShaderTest::TearDown();
  }

  void GenVertexArraysOESImmediateValidArgs() {
    AddExpectationsForGenVertexArraysOES();
    auto* cmd = GetImmediateAs<cmds::GenVertexArraysOESImmediate>();
    GLuint temp = kNewClientId;
    cmd->Init(1, &temp);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(*cmd, sizeof(temp)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(GetVertexArrayInfo(kNewClientId) != nullptr);
    AddExpectationsForDeleteVertexArraysOES();
  }

  void GenVertexArraysOESImmediateDuplicateOrNullIds() {
    auto* cmd = GetImmediateAs<cmds::GenVertexArraysOESImmediate>();
    GLuint temp[3] = {kNewClientId, kNewClientId + 1, kNewClientId};
    cmd->Init(3, temp);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(*cmd, sizeof(temp)));
    EXPECT_TRUE(GetVertexArrayInfo(kNewClientId) == nullptr);
    EXPECT_TRUE(GetVertexArrayInfo(kNewClientId + 1) == nullptr);
    GLuint null_id[2] = {kNewClientId, 0};
    cmd->Init(2, null_id);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(*cmd, sizeof(temp)));
    EXPECT_TRUE(GetVertexArrayInfo(kNewClientId) == nullptr);
  }

  void GenVertexArraysOESImmediateInvalidArgs() {
    EXPECT_CALL(*gl_, GenVertexArraysOES(_, _)).Times(0);
    auto* cmd = GetImmediateAs<cmds::GenVertexArraysOESImmediate>();
    cmd->Init(1, &client_vertexarray_id_);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(*cmd, sizeof(&client_vertexarray_id_)));
  }

  void DeleteVertexArraysOESImmediateValidArgs() {
    AddExpectationsForDeleteVertexArraysOES();
    auto& cmd = *GetImmediateAs<cmds::DeleteVertexArraysOESImmediate>();
    cmd.Init(1, &client_vertexarray_id_);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_vertexarray_id_)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(GetVertexArrayInfo(client_vertexarray_id_) == nullptr);
    vertex_array_deleted_manually_ = true;
  }

  void DeleteVertexArraysOESImmediateInvalidArgs() {
    auto& cmd = *GetImmediateAs<cmds::DeleteVertexArraysOESImmediate>();
    GLuint temp = kInvalidClientId;
    cmd.Init(1, &temp);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  }

  void DeleteBoundVertexArraysOESImmediateValidArgs() {
    BindVertexArrayOESValidArgs();

    AddExpectationsForDeleteBoundVertexArraysOES();
    auto& cmd = *GetImmediateAs<cmds::DeleteVertexArraysOESImmediate>();
    cmd.Init(1, &client_vertexarray_id_);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_vertexarray_id_)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(GetVertexArrayInfo(client_vertexarray_id_) == nullptr);
    vertex_array_deleted_manually_ = true;
  }

  void IsVertexArrayOESValidArgs() {
    cmds::IsVertexArrayOES cmd;
    cmd.Init(client_vertexarray_id_, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void IsVertexArrayOESInvalidArgsBadSharedMemoryId() {
    cmds::IsVertexArrayOES cmd;
    cmd.Init(
        client_vertexarray_id_, kInvalidSharedMemoryId, shared_memory_offset_);
    EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
    cmd.Init(
        client_vertexarray_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
    EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  }

  void BindVertexArrayOESValidArgs() {
    AddExpectationsForBindVertexArrayOES();
    cmds::BindVertexArrayOES cmd;
    cmd.Init(client_vertexarray_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void BindVertexArrayOESValidArgsNewId() {
    cmds::BindVertexArrayOES cmd;
    cmd.Init(kNewClientId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderVertexArraysOESTest,
                         ::testing::Bool());

class GLES2DecoderEmulatedVertexArraysOESTest
    : public GLES2DecoderVertexArraysOESTest {
 public:
  GLES2DecoderEmulatedVertexArraysOESTest() = default;

  void SetUp() override {
    InitState init;
    init.bind_generates_resource = true;
    init.use_native_vao = false;
    InitDecoder(init);
    SetupDefaultProgram();

    AddExpectationsForGenVertexArraysOES();
    GenHelper<cmds::GenVertexArraysOESImmediate>(client_vertexarray_id_);

    vertex_array_deleted_manually_ = false;
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderEmulatedVertexArraysOESTest,
                         ::testing::Bool());

// Test vertex array objects with native support
TEST_P(GLES2DecoderVertexArraysOESTest, GenVertexArraysOESImmediateValidArgs) {
  GenVertexArraysOESImmediateValidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       GenVertexArraysOESImmediateValidArgs) {
  GenVertexArraysOESImmediateValidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest,
       GenVertexArraysOESImmediateDuplicateOrNullIds) {
  GenVertexArraysOESImmediateDuplicateOrNullIds();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       GenVertexArraysOESImmediateDuplicateOrNullIds) {
  GenVertexArraysOESImmediateDuplicateOrNullIds();
}

TEST_P(GLES2DecoderVertexArraysOESTest,
       GenVertexArraysOESImmediateInvalidArgs) {
  GenVertexArraysOESImmediateInvalidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       GenVertexArraysOESImmediateInvalidArgs) {
  GenVertexArraysOESImmediateInvalidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest,
       DeleteVertexArraysOESImmediateValidArgs) {
  DeleteVertexArraysOESImmediateValidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       DeleteVertexArraysOESImmediateValidArgs) {
  DeleteVertexArraysOESImmediateValidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest,
       DeleteVertexArraysOESImmediateInvalidArgs) {
  DeleteVertexArraysOESImmediateInvalidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       DeleteVertexArraysOESImmediateInvalidArgs) {
  DeleteVertexArraysOESImmediateInvalidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest,
       DeleteBoundVertexArraysOESImmediateValidArgs) {
  DeleteBoundVertexArraysOESImmediateValidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       DeleteBoundVertexArraysOESImmediateValidArgs) {
  DeleteBoundVertexArraysOESImmediateValidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest, IsVertexArrayOESValidArgs) {
  IsVertexArrayOESValidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest, IsVertexArrayOESValidArgs) {
  IsVertexArrayOESValidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest,
       IsVertexArrayOESInvalidArgsBadSharedMemoryId) {
  IsVertexArrayOESInvalidArgsBadSharedMemoryId();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       IsVertexArrayOESInvalidArgsBadSharedMemoryId) {
  IsVertexArrayOESInvalidArgsBadSharedMemoryId();
}

TEST_P(GLES2DecoderVertexArraysOESTest, BindVertexArrayOESValidArgs) {
  BindVertexArrayOESValidArgs();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest, BindVertexArrayOESValidArgs) {
  BindVertexArrayOESValidArgs();
}

TEST_P(GLES2DecoderVertexArraysOESTest, BindVertexArrayOESValidArgsNewId) {
  BindVertexArrayOESValidArgsNewId();
}
TEST_P(GLES2DecoderEmulatedVertexArraysOESTest,
       BindVertexArrayOESValidArgsNewId) {
  BindVertexArrayOESValidArgsNewId();
}

TEST_P(GLES2DecoderTest, BufferDataGLError) {
  GLenum target = GL_ARRAY_BUFFER;
  GLsizeiptr size = 4;
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  BufferManager* manager = group().buffer_manager();
  Buffer* buffer = manager->GetBuffer(client_buffer_id_);
  ASSERT_TRUE(buffer != nullptr);
  EXPECT_EQ(0, buffer->size());
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(target, size, _, GL_STREAM_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BufferData cmd;
  cmd.Init(target, size, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_EQ(0, buffer->size());
}

// TODO(gman): BufferData

// TODO(gman): BufferDataImmediate

// TODO(gman): BufferSubData

// TODO(gman): BufferSubDataImmediate

}  // namespace gles2
}  // namespace gpu
