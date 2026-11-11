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

namespace gpu::gles2 {

ProtectedCommandPolicy GetProtectedCommandPolicy(unsigned int command) {
  switch (command) {
    // State, objects, shaders and programs.
    case kActiveTexture:
    case kAttachShader:
    case kBindAttribLocationBucket:
    case kBindBuffer:
    case kBindBufferBase:
    case kBindBufferRange:
    case kBindFragDataLocationEXTBucket:
    case kBindFragDataLocationIndexedEXTBucket:
    case kBindFramebuffer:
    case kBindRenderbuffer:
    case kBindSampler:
    case kBindTexture:
    case kBindTransformFeedback:
    case kBindUniformLocationCHROMIUMBucket:
    case kBindVertexArrayOES:
    case kBlendBarrierKHR:
    case kBlendColor:
    case kBlendEquation:
    case kBlendEquationSeparate:
    case kBlendEquationSeparateiOES:
    case kBlendEquationiOES:
    case kBlendFunc:
    case kBlendFuncSeparate:
    case kBlendFuncSeparateiOES:
    case kBlendFunciOES:
    case kClearColor:
    case kClearDepthf:
    case kClearStencil:
    case kClipControlEXT:
    case kColorMask:
    case kColorMaskiOES:
    case kCompileShader:
    case kCreateProgram:
    case kCreateShader:
    case kCullFace:
    case kDeleteBuffersImmediate:
    case kDeleteFramebuffersImmediate:
    case kDeleteProgram:
    case kDeleteQueriesEXTImmediate:
    case kDeleteRenderbuffersImmediate:
    case kDeleteSamplersImmediate:
    case kDeleteShader:
    case kDeleteSync:
    case kDeleteTexturesImmediate:
    case kDeleteTransformFeedbacksImmediate:
    case kDeleteVertexArraysOESImmediate:
    case kDepthFunc:
    case kDepthMask:
    case kDepthRangef:
    case kDetachShader:
    case kDisable:
    case kDisableVertexAttribArray:
    case kDisableiOES:
    case kDrawBuffersEXTImmediate:
    case kEnable:
    case kEnableFeatureCHROMIUM:
    case kEnableVertexAttribArray:
    case kEnableiOES:
    case kEndQueryEXT:
    case kFramebufferRenderbuffer:
    case kFrontFace:
    case kGenBuffersImmediate:
    case kGenFramebuffersImmediate:
    case kGenQueriesEXTImmediate:
    case kGenRenderbuffersImmediate:
    case kGenSamplersImmediate:
    case kGenTexturesImmediate:
    case kGenTransformFeedbacksImmediate:
    case kGenVertexArraysOESImmediate:
    case kHint:
    case kLineWidth:
    case kLinkProgram:
    case kMaxShaderCompilerThreadsKHR:
    case kPixelStorei:
    case kPolygonModeANGLE:
    case kPolygonOffset:
    case kPolygonOffsetClampEXT:
    case kProvokingVertexANGLE:
    case kReadBuffer:
    case kReleaseShaderCompiler:
    case kRenderbufferStorage:
    case kRenderbufferStorageMultisampleAdvancedAMD:
    case kRenderbufferStorageMultisampleCHROMIUM:
    case kRenderbufferStorageMultisampleEXT:
    case kRequestExtensionCHROMIUM:
    case kSampleCoverage:
    case kSamplerParameterf:
    case kSamplerParameterfvImmediate:
    case kSamplerParameteri:
    case kSamplerParameterivImmediate:
    case kScissor:
    case kShaderSourceBucket:
    case kStencilFunc:
    case kStencilFuncSeparate:
    case kStencilMask:
    case kStencilMaskSeparate:
    case kStencilOp:
    case kStencilOpSeparate:
    case kTexParameterf:
    case kTexParameterfvImmediate:
    case kTexParameteri:
    case kTexParameterivImmediate:
    case kTexStorage2DEXT:
    case kTexStorage3D:
    case kTransformFeedbackVaryingsBucket:
    case kUniform1f:
    case kUniform1fvImmediate:
    case kUniform1i:
    case kUniform1ivImmediate:
    case kUniform1ui:
    case kUniform1uivImmediate:
    case kUniform2f:
    case kUniform2fvImmediate:
    case kUniform2i:
    case kUniform2ivImmediate:
    case kUniform2ui:
    case kUniform2uivImmediate:
    case kUniform3f:
    case kUniform3fvImmediate:
    case kUniform3i:
    case kUniform3ivImmediate:
    case kUniform3ui:
    case kUniform3uivImmediate:
    case kUniform4f:
    case kUniform4fvImmediate:
    case kUniform4i:
    case kUniform4ivImmediate:
    case kUniform4ui:
    case kUniform4uivImmediate:
    case kUniformBlockBinding:
    case kUniformMatrix2fvImmediate:
    case kUniformMatrix2x3fvImmediate:
    case kUniformMatrix2x4fvImmediate:
    case kUniformMatrix3fvImmediate:
    case kUniformMatrix3x2fvImmediate:
    case kUniformMatrix3x4fvImmediate:
    case kUniformMatrix4fvImmediate:
    case kUniformMatrix4x2fvImmediate:
    case kUniformMatrix4x3fvImmediate:
    case kUseProgram:
    case kValidateProgram:
    case kVertexAttrib1f:
    case kVertexAttrib1fvImmediate:
    case kVertexAttrib2f:
    case kVertexAttrib2fvImmediate:
    case kVertexAttrib3f:
    case kVertexAttrib3fvImmediate:
    case kVertexAttrib4f:
    case kVertexAttrib4fvImmediate:
    case kVertexAttribDivisorANGLE:
    case kVertexAttribI4i:
    case kVertexAttribI4ivImmediate:
    case kVertexAttribI4ui:
    case kVertexAttribI4uivImmediate:
    case kVertexAttribIPointer:
    case kVertexAttribPointer:
    case kViewport:
    case kWindowRectanglesEXTImmediate:
    // Data from the client into GPU objects, copies between GPU objects, and
    // shared images opened for reading.
    case kBufferData:
    case kBufferSubData:
    case kCompressedTexImage2D:
    case kCompressedTexImage2DBucket:
    case kCompressedTexImage3D:
    case kCompressedTexImage3DBucket:
    case kCopyBufferSubData:
    case kCopySharedImageToTextureINTERNALImmediate:
    case kCreateAndTexStorage2DSharedImageINTERNALImmediate:
    case kEndSharedImageAccessDirectCHROMIUM:
    case kFlushMappedBufferRange:
    case kGenerateMipmap:
    case kTexImage2D:
    case kTexImage3D:
    case kUnmapBuffer:
    // Drawing.
    case kBeginTransformFeedback:
    case kClear:
    case kClearBufferfi:
    case kClearBufferfvImmediate:
    case kClearBufferivImmediate:
    case kClearBufferuivImmediate:
    case kDiscardFramebufferEXTImmediate:
    case kDrawArrays:
    case kDrawArraysInstancedANGLE:
    case kDrawArraysInstancedBaseInstanceANGLE:
    case kDrawElements:
    case kDrawElementsInstancedANGLE:
    case kDrawElementsInstancedBaseVertexBaseInstanceANGLE:
    case kEndTransformFeedback:
    case kInvalidateFramebufferImmediate:
    case kInvalidateSubFramebufferImmediate:
    case kMultiDrawArraysCHROMIUM:
    case kMultiDrawArraysInstancedBaseInstanceCHROMIUM:
    case kMultiDrawArraysInstancedCHROMIUM:
    case kMultiDrawBeginCHROMIUM:
    case kMultiDrawElementsCHROMIUM:
    case kMultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM:
    case kMultiDrawElementsInstancedCHROMIUM:
    case kMultiDrawEndCHROMIUM:
    case kPauseTransformFeedback:
    case kPresentProtectedFrameCOBALT:
    case kResumeTransformFeedback:
    // Synchronization, lifetime and diagnostics.
    case kClientWaitSync:
    case kContextVisibilityHintCHROMIUM:
    case kCreateGpuFenceINTERNAL:
    case kDescheduleUntilFinishedCHROMIUM:
    case kDestroyGpuFenceCHROMIUM:
    case kFenceSync:
    case kFinish:
    case kFlush:
    case kFlushDriverCachesCHROMIUM:
    case kInitializeDiscardableTextureCHROMIUM:
    case kInsertEventMarkerEXT:
    case kLockDiscardableTextureCHROMIUM:
    case kLoseContextCHROMIUM:
    case kPopGroupMarkerEXT:
    case kPushGroupMarkerEXT:
    case kSetActiveURLCHROMIUM:
    case kTraceBeginCHROMIUM:
    case kTraceEndCHROMIUM:
    case kUnlockDiscardableTextureCHROMIUM:
    case kWaitGpuFenceCHROMIUM:
    case kWaitSync:
    // Queries of state the client set, or of shaders and programs; none
    // returns texels or values computed from them.
    case kCheckFramebufferStatus:
    case kGetActiveAttrib:
    case kGetActiveUniform:
    case kGetActiveUniformBlockName:
    case kGetActiveUniformBlockiv:
    case kGetActiveUniformsiv:
    case kGetAttachedShaders:
    case kGetAttribLocation:
    case kGetBooleani_v:
    case kGetBooleanv:
    case kGetBufferParameteri64v:
    case kGetBufferParameteriv:
    case kGetError:
    case kGetFloatv:
    case kGetFragDataIndexEXT:
    case kGetFragDataLocation:
    case kGetFramebufferAttachmentParameteriv:
    case kGetInteger64i_v:
    case kGetInteger64v:
    case kGetIntegeri_v:
    case kGetIntegerv:
    case kGetInternalformativ:
    case kGetProgramInfoCHROMIUM:
    case kGetProgramInfoLog:
    case kGetProgramiv:
    case kGetRenderbufferParameteriv:
    case kGetRequestableExtensionsCHROMIUM:
    case kGetSamplerParameterfv:
    case kGetSamplerParameteriv:
    case kGetShaderInfoLog:
    case kGetShaderPrecisionFormat:
    case kGetShaderSource:
    case kGetShaderiv:
    case kGetString:
    case kGetSynciv:
    case kGetTexParameterfv:
    case kGetTexParameteriv:
    case kGetTransformFeedbackVarying:
    case kGetTransformFeedbackVaryingsCHROMIUM:
    case kGetTranslatedShaderSourceANGLE:
    case kGetUniformBlockIndex:
    case kGetUniformBlocksCHROMIUM:
    case kGetUniformIndices:
    case kGetUniformLocation:
    case kGetUniformfv:
    case kGetUniformiv:
    case kGetUniformsES3CHROMIUM:
    case kGetUniformuiv:
    case kGetVertexAttribIiv:
    case kGetVertexAttribIuiv:
    case kGetVertexAttribPointerv:
    case kGetVertexAttribfv:
    case kGetVertexAttribiv:
    case kIsBuffer:
    case kIsEnabled:
    case kIsEnablediOES:
    case kIsFramebuffer:
    case kIsProgram:
    case kIsRenderbuffer:
    case kIsSampler:
    case kIsShader:
    case kIsSync:
    case kIsTexture:
    case kIsTransformFeedback:
    case kIsVertexArrayOES:
      return ProtectedCommandPolicy::kAllowed;

    // Only queries that report completion.
    case kBeginQueryEXT:
    // Only read access to shared images.
    case kBeginSharedImageAccessDirectCHROMIUM:
    // Not into a texture backed by a shared image, which other contexts can
    // read.
    case kTexSubImage2D:
    case kTexSubImage3D:
    case kCompressedTexSubImage2D:
    case kCompressedTexSubImage2DBucket:
    case kCompressedTexSubImage3D:
    case kCompressedTexSubImage3DBucket:
    case kCopySubTextureCHROMIUM:
    case kCopyTextureCHROMIUM:
    case kFramebufferTexture2D:
    case kFramebufferTexture2DMultisampleEXT:
    case kFramebufferTextureLayer:
    case kFramebufferTextureMultiviewOVR:
    // Not from the protected default framebuffer.
    case kBlitFramebufferCHROMIUM:
      return ProtectedCommandPolicy::kAllowedWithArgumentChecks;

    // Read framebuffer or texture contents back to the client, or copy the
    // protected framebuffer into a texture.
    case kCopyTexImage2D:
    case kCopyTexSubImage2D:
    case kCopyTexSubImage3D:
    case kReadPixels:
    case kReadbackARGBImagePixelsINTERNAL:
    // Read buffer contents back to the client. Shaders can write values
    // computed from protected texels into buffers with transform feedback.
    case kGetMaxValueInBufferCHROMIUM:
    case kMapBufferRange:
    case kSetReadbackBufferShadowAllocationINTERNAL:
    // GPU timestamps, which can encode texels in a shader's running time.
    case kQueryCounterEXT:
    case kSetDisjointValueSyncCHROMIUM:
    // Write a shared image that other contexts can read.
    case kCopySharedImageINTERNALImmediate:
    case kWritePixelsYUVINTERNAL:
    // Not reachable from WebGL: OpenGL ES 3.1 compute and indirect drawing,
    // pixel local storage (a draft extension), and shader binaries.
    case kBeginPixelLocalStorageANGLEImmediate:
    case kBindImageTexture:
    case kDispatchCompute:
    case kDispatchComputeIndirect:
    case kDrawArraysIndirect:
    case kDrawElementsIndirect:
    case kEndPixelLocalStorageANGLEImmediate:
    case kFramebufferMemorylessPixelLocalStorageANGLE:
    case kFramebufferParameteri:
    case kFramebufferPixelLocalClearValuefvANGLEImmediate:
    case kFramebufferPixelLocalClearValueivANGLEImmediate:
    case kFramebufferPixelLocalClearValueuivANGLEImmediate:
    case kFramebufferPixelLocalStorageInterruptANGLE:
    case kFramebufferPixelLocalStorageRestoreANGLE:
    case kFramebufferTexturePixelLocalStorageANGLE:
    case kGetFramebufferPixelLocalStorageParameterfvANGLE:
    case kGetFramebufferPixelLocalStorageParameterivANGLE:
    case kGetProgramInterfaceiv:
    case kGetProgramResourceIndex:
    case kGetProgramResourceLocation:
    case kGetProgramResourceName:
    case kGetProgramResourceiv:
    case kMemoryBarrierByRegion:
    case kMemoryBarrierEXT:
    case kPixelLocalStorageBarrierANGLE:
    case kShaderBinary:
      return ProtectedCommandPolicy::kDenied;

    default:
      return ProtectedCommandPolicy::kUnclassified;
  }
}

bool IsCommandAllowedInProtectedContext(unsigned int command) {
  const ProtectedCommandPolicy policy = GetProtectedCommandPolicy(command);
  return policy == ProtectedCommandPolicy::kAllowed ||
         policy == ProtectedCommandPolicy::kAllowedWithArgumentChecks;
}

bool IsQueryTargetAllowedInProtectedContext(GLenum target) {
  switch (target) {
    case GL_COMMANDS_COMPLETED_CHROMIUM:
    case GL_GET_ERROR_QUERY_CHROMIUM:
    case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
    // WebGL fenceSync() issues this query. The buffer copies it would
    // trigger need SetReadbackBufferShadowAllocationINTERNAL, which is denied.
    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
    // The count depends only on the draw calls, not on shader results.
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
      return true;
    default:
      return false;
  }
}

bool IsSharedImageAccessModeAllowedInProtectedContext(GLenum mode) {
  return mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM;
}

}  // namespace gpu::gles2
