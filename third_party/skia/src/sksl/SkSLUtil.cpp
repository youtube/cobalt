/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLUtil.h"

#include "src/sksl/SkSLContext.h"
#include "src/sksl/SkSLStringStream.h"
#include "src/sksl/ir/SkSLType.h"

#include "src/gpu/GrShaderCaps.h"

#if defined(STARBOARD)
#include "starboard/system.h"
#define exit(status) SbSystemBreakIntoDebugger()
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

namespace SkSL {

#if defined(SKSL_STANDALONE) || !SK_SUPPORT_GPU
std::unique_ptr<ShaderCaps> ShaderCapsFactory::MakeShaderCaps() {
    std::unique_ptr<ShaderCaps> standalone = std::make_unique<ShaderCaps>();
    standalone->fShaderDerivativeSupport = true;
    standalone->fFlatInterpolationSupport = true;
    standalone->fNoPerspectiveInterpolationSupport = true;
    standalone->fSampleMaskSupport = true;
    standalone->fExternalTextureSupport = true;
    // we define canUseDoLoops to false in standalone so we don't use do loops while inlining
    // in FP files (which would then, being baked in, end up being used even in contexts where
    // do loops are not allowed)
    standalone->fCanUseDoLoops = false;
    return standalone;
}
#else
std::unique_ptr<ShaderCaps> ShaderCapsFactory::MakeShaderCaps() {
    return std::make_unique<GrShaderCaps>();
}
#endif  // defined(SKSL_STANDALONE) || !SK_SUPPORT_GPU

void write_stringstream(const StringStream& s, OutputStream& out) {
    out.write(s.str().c_str(), s.str().size());
}

#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
bool type_to_grsltype(const Context& context, const Type& type, GrSLType* outType) {
    // If a new GrSL type is added, this function will need to be updated.
    static_assert(kGrSLTypeCount == 41);

    if (type == *context.fTypes.fVoid    ) { *outType = kVoid_GrSLType;     return true; }
    if (type == *context.fTypes.fBool    ) { *outType = kBool_GrSLType;     return true; }
    if (type == *context.fTypes.fBool2   ) { *outType = kBool2_GrSLType;    return true; }
    if (type == *context.fTypes.fBool3   ) { *outType = kBool3_GrSLType;    return true; }
    if (type == *context.fTypes.fBool4   ) { *outType = kBool4_GrSLType;    return true; }
    if (type == *context.fTypes.fShort   ) { *outType = kShort_GrSLType;    return true; }
    if (type == *context.fTypes.fShort2  ) { *outType = kShort2_GrSLType;   return true; }
    if (type == *context.fTypes.fShort3  ) { *outType = kShort3_GrSLType;   return true; }
    if (type == *context.fTypes.fShort4  ) { *outType = kShort4_GrSLType;   return true; }
    if (type == *context.fTypes.fUShort  ) { *outType = kUShort_GrSLType;   return true; }
    if (type == *context.fTypes.fUShort2 ) { *outType = kUShort2_GrSLType;  return true; }
    if (type == *context.fTypes.fUShort3 ) { *outType = kUShort3_GrSLType;  return true; }
    if (type == *context.fTypes.fUShort4 ) { *outType = kUShort4_GrSLType;  return true; }
    if (type == *context.fTypes.fFloat   ) { *outType = kFloat_GrSLType;    return true; }
    if (type == *context.fTypes.fFloat2  ) { *outType = kFloat2_GrSLType;   return true; }
    if (type == *context.fTypes.fFloat3  ) { *outType = kFloat3_GrSLType;   return true; }
    if (type == *context.fTypes.fFloat4  ) { *outType = kFloat4_GrSLType;   return true; }
    if (type == *context.fTypes.fFloat2x2) { *outType = kFloat2x2_GrSLType; return true; }
    if (type == *context.fTypes.fFloat3x3) { *outType = kFloat3x3_GrSLType; return true; }
    if (type == *context.fTypes.fFloat4x4) { *outType = kFloat4x4_GrSLType; return true; }
    if (type == *context.fTypes.fHalf    ) { *outType = kHalf_GrSLType;     return true; }
    if (type == *context.fTypes.fHalf2   ) { *outType = kHalf2_GrSLType;    return true; }
    if (type == *context.fTypes.fHalf3   ) { *outType = kHalf3_GrSLType;    return true; }
    if (type == *context.fTypes.fHalf4   ) { *outType = kHalf4_GrSLType;    return true; }
    if (type == *context.fTypes.fHalf2x2 ) { *outType = kHalf2x2_GrSLType;  return true; }
    if (type == *context.fTypes.fHalf3x3 ) { *outType = kHalf3x3_GrSLType;  return true; }
    if (type == *context.fTypes.fHalf4x4 ) { *outType = kHalf4x4_GrSLType;  return true; }
    if (type == *context.fTypes.fInt     ) { *outType = kInt_GrSLType;      return true; }
    if (type == *context.fTypes.fInt2    ) { *outType = kInt2_GrSLType;     return true; }
    if (type == *context.fTypes.fInt3    ) { *outType = kInt3_GrSLType;     return true; }
    if (type == *context.fTypes.fInt4    ) { *outType = kInt4_GrSLType;     return true; }
    if (type == *context.fTypes.fUInt    ) { *outType = kUInt_GrSLType;     return true; }
    if (type == *context.fTypes.fUInt2   ) { *outType = kUInt2_GrSLType;    return true; }
    if (type == *context.fTypes.fUInt3   ) { *outType = kUInt3_GrSLType;    return true; }
    if (type == *context.fTypes.fUInt4   ) { *outType = kUInt4_GrSLType;    return true; }
    return false;
}
#endif

}  // namespace SkSL
