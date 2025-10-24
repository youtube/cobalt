//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Scalarize vector and matrix constructor args, so that vectors built from components don't have
// matrix arguments, and matrices built from components don't have vector arguments. This avoids
// driver bugs around vector and matrix constructors.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_GLSL_SCALARIZEVECANDMATCONSTRUCTORARGS_H_
#define COMPILER_TRANSLATOR_TREEOPS_GLSL_SCALARIZEVECANDMATCONSTRUCTORARGS_H_

#include "GLSLANG/ShaderLang.h"
#include "common/angleutils.h"
#include "common/debug.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool ScalarizeVecAndMatConstructorArgs(TCompiler *compiler,
                                                     TIntermBlock *root,
                                                     TSymbolTable *symbolTable);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_GLSL_SCALARIZEVECANDMATCONSTRUCTORARGS_H_
