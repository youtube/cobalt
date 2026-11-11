// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "gpu/command_buffer/service/cobalt_protected_commands.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include "gpu/command_buffer/common/gles2_cmd_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu::gles2 {
namespace {

// Fails when a command is added to gles2_cmd_buffer_functions.txt (for example
// by an upstream rebase) until it is classified in
// cobalt_protected_commands.cc. Unclassified commands are refused at runtime.
TEST(CobaltProtectedCommandsTest, EveryCommandIsClassified) {
  for (unsigned int command = kFirstGLES2Command; command < kNumCommands;
       ++command) {
    EXPECT_NE(GetProtectedCommandPolicy(command),
              ProtectedCommandPolicy::kUnclassified)
        << GetCommandName(static_cast<CommandId>(command))
        << " needs a protected-context policy.";
  }
}

TEST(CobaltProtectedCommandsTest, UnknownCommandsAreRefused) {
  EXPECT_EQ(GetProtectedCommandPolicy(kNumCommands),
            ProtectedCommandPolicy::kUnclassified);
  EXPECT_FALSE(IsCommandAllowedInProtectedContext(kNumCommands));
}

TEST(CobaltProtectedCommandsTest, ReadbackIsRefused) {
  for (unsigned int command :
       {kReadPixels, kReadbackARGBImagePixelsINTERNAL, kCopyTexImage2D,
        kCopyTexSubImage2D, kCopyTexSubImage3D, kMapBufferRange,
        kSetReadbackBufferShadowAllocationINTERNAL, kGetMaxValueInBufferCHROMIUM,
        kQueryCounterEXT, kCopySharedImageINTERNALImmediate}) {
    EXPECT_FALSE(IsCommandAllowedInProtectedContext(command))
        << GetCommandName(static_cast<CommandId>(command));
  }
}

TEST(CobaltProtectedCommandsTest, RenderingIsAllowed) {
  for (unsigned int command :
       {kDrawArrays, kDrawElements, kTexImage2D, kUseProgram, kFlush,
        kPresentProtectedFrameCOBALT,
        kCreateAndTexStorage2DSharedImageINTERNALImmediate}) {
    EXPECT_EQ(GetProtectedCommandPolicy(command),
              ProtectedCommandPolicy::kAllowed)
        << GetCommandName(static_cast<CommandId>(command));
  }
}

// These need the checks in the passthrough decoder's doers; keep the two in
// sync.
TEST(CobaltProtectedCommandsTest, ArgumentCheckedCommands) {
  for (unsigned int command :
       {kBeginQueryEXT, kBeginSharedImageAccessDirectCHROMIUM,
        kBlitFramebufferCHROMIUM, kCopySubTextureCHROMIUM, kCopyTextureCHROMIUM,
        kFramebufferTexture2D, kFramebufferTexture2DMultisampleEXT,
        kFramebufferTextureLayer, kFramebufferTextureMultiviewOVR,
        kTexSubImage2D, kTexSubImage3D, kCompressedTexSubImage2D,
        kCompressedTexSubImage2DBucket, kCompressedTexSubImage3D,
        kCompressedTexSubImage3DBucket}) {
    EXPECT_EQ(GetProtectedCommandPolicy(command),
              ProtectedCommandPolicy::kAllowedWithArgumentChecks)
        << GetCommandName(static_cast<CommandId>(command));
    EXPECT_TRUE(IsCommandAllowedInProtectedContext(command));
  }
}

TEST(CobaltProtectedCommandsTest, OnlyCompletionQueriesAreAllowed) {
  EXPECT_TRUE(
      IsQueryTargetAllowedInProtectedContext(GL_COMMANDS_COMPLETED_CHROMIUM));
  EXPECT_TRUE(IsQueryTargetAllowedInProtectedContext(
      GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM));
  EXPECT_FALSE(IsQueryTargetAllowedInProtectedContext(GL_ANY_SAMPLES_PASSED));
  EXPECT_FALSE(IsQueryTargetAllowedInProtectedContext(
      GL_ANY_SAMPLES_PASSED_CONSERVATIVE));
  EXPECT_FALSE(IsQueryTargetAllowedInProtectedContext(GL_TIME_ELAPSED_EXT));
  EXPECT_FALSE(IsQueryTargetAllowedInProtectedContext(GL_TIMESTAMP_EXT));
  EXPECT_FALSE(
      IsQueryTargetAllowedInProtectedContext(GL_COMMANDS_ISSUED_CHROMIUM));
}

TEST(CobaltProtectedCommandsTest, SharedImagesAreReadOnly) {
  EXPECT_TRUE(IsSharedImageAccessModeAllowedInProtectedContext(
      GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));
  EXPECT_FALSE(IsSharedImageAccessModeAllowedInProtectedContext(
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM));
}

}  // namespace
}  // namespace gpu::gles2
