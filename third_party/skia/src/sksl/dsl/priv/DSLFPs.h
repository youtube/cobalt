/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_DSL_FPS
#define SKSL_DSL_FPS

#include "include/sksl/DSL.h"

#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU

#include "src/gpu/GrFragmentProcessor.h"

namespace SkSL {

namespace dsl {

void StartFragmentProcessor(GrFragmentProcessor::ProgramImpl* processor,
                            GrFragmentProcessor::ProgramImpl::EmitArgs* emitArgs);

void EndFragmentProcessor();

DSLGlobalVar sk_SampleCoord();

DSLExpression SampleChild(int index, DSLExpression coords = DSLExpression());

GrGLSLUniformHandler::UniformHandle VarUniformHandle(const DSLGlobalVar& var);

} // namespace dsl

} // namespace SkSL

#endif // !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU

#endif
