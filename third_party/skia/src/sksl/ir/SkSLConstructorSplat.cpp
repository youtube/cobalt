/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLConstantFolder.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/ir/SkSLConstructorSplat.h"

namespace SkSL {

std::unique_ptr<Expression> ConstructorSplat::Make(const Context& context,
                                                   int line,
                                                   const Type& type,
                                                   std::unique_ptr<Expression> arg) {
    SkASSERT(type.isAllowedInES2(context));
    SkASSERT(type.isScalar() || type.isVector());
    SkASSERT(arg->type().scalarTypeForLiteral().matches(
            type.componentType().scalarTypeForLiteral()));
    SkASSERT(arg->type().isScalar());

    // A "splat" to a scalar type is a no-op and can be eliminated.
    if (type.isScalar()) {
        return arg;
    }

    // Replace constant variables with their corresponding values, so `float3(five)` can compile
    // down to `float3(5.0)` (the latter is a compile-time constant).
    arg = ConstantFolder::MakeConstantValueForVariable(std::move(arg));

    SkASSERT(type.isVector());
    return std::make_unique<ConstructorSplat>(line, type, std::move(arg));
}

}  // namespace SkSL
