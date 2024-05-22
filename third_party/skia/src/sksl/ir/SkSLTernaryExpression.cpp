/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLTernaryExpression.h"

#include "include/sksl/SkSLErrorReporter.h"
#include "src/sksl/SkSLConstantFolder.h"
#include "src/sksl/SkSLContext.h"
#include "src/sksl/SkSLOperators.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/ir/SkSLLiteral.h"

namespace SkSL {

std::unique_ptr<Expression> TernaryExpression::Convert(const Context& context,
                                                       std::unique_ptr<Expression> test,
                                                       std::unique_ptr<Expression> ifTrue,
                                                       std::unique_ptr<Expression> ifFalse) {
    test = context.fTypes.fBool->coerceExpression(std::move(test), context);
    if (!test || !ifTrue || !ifFalse) {
        return nullptr;
    }
    int line = test->fLine;
    const Type* trueType;
    const Type* falseType;
    const Type* resultType;
    Operator equalityOp(Token::Kind::TK_EQEQ);
    if (!equalityOp.determineBinaryType(context, ifTrue->type(), ifFalse->type(),
                                        &trueType, &falseType, &resultType) ||
        !trueType->matches(*falseType)) {
        context.fErrors->error(line, "ternary operator result mismatch: '" +
                                     ifTrue->type().displayName() + "', '" +
                                     ifFalse->type().displayName() + "'");
        return nullptr;
    }
    if (trueType->componentType().isOpaque()) {
        context.fErrors->error(line, "ternary expression of opaque type '" +
                                     trueType->displayName() + "' not allowed");
        return nullptr;
    }
    if (context.fConfig->strictES2Mode() && trueType->isOrContainsArray()) {
        context.fErrors->error(line, "ternary operator result may not be an array (or struct "
                                     "containing an array)");
        return nullptr;
    }
    ifTrue = trueType->coerceExpression(std::move(ifTrue), context);
    if (!ifTrue) {
        return nullptr;
    }
    ifFalse = falseType->coerceExpression(std::move(ifFalse), context);
    if (!ifFalse) {
        return nullptr;
    }
    return TernaryExpression::Make(context, std::move(test), std::move(ifTrue), std::move(ifFalse));
}

std::unique_ptr<Expression> TernaryExpression::Make(const Context& context,
                                                    std::unique_ptr<Expression> test,
                                                    std::unique_ptr<Expression> ifTrue,
                                                    std::unique_ptr<Expression> ifFalse) {
    SkASSERT(ifTrue->type().matches(ifFalse->type()));
    SkASSERT(!ifTrue->type().componentType().isOpaque());
    SkASSERT(!context.fConfig->strictES2Mode() || !ifTrue->type().isOrContainsArray());

    const Expression* testExpr = ConstantFolder::GetConstantValueForVariable(*test);
    if (testExpr->isBoolLiteral()) {
        // static boolean test, just return one of the branches
        return testExpr->as<Literal>().boolValue() ? std::move(ifTrue)
                                                   : std::move(ifFalse);
    }

    return std::make_unique<TernaryExpression>(test->fLine, std::move(test), std::move(ifTrue),
                                               std::move(ifFalse));
}

}  // namespace SkSL
