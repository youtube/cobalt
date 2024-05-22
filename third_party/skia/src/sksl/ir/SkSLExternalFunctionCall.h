/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_EXTERNALFUNCTIONCALL
#define SKSL_EXTERNALFUNCTIONCALL

#include "include/private/SkTArray.h"
#include "src/sksl/ir/SkSLExpression.h"
#include "src/sksl/ir/SkSLExternalFunction.h"
#include "src/sksl/ir/SkSLFunctionDeclaration.h"

namespace SkSL {

/**
 * An external function invocation.
 */
class ExternalFunctionCall final : public Expression {
public:
    inline static constexpr Kind kExpressionKind = Kind::kExternalFunctionCall;

    ExternalFunctionCall(int line, const ExternalFunction* function, ExpressionArray arguments)
        : INHERITED(line, kExpressionKind, &function->type())
        , fFunction(*function)
        , fArguments(std::move(arguments)) {}

    ExpressionArray& arguments() {
        return fArguments;
    }

    const ExpressionArray& arguments() const {
        return fArguments;
    }

    const ExternalFunction& function() const {
        return fFunction;
    }

    bool hasProperty(Property property) const override {
        if (property == Property::kSideEffects) {
            return true;
        }
        for (const auto& arg : this->arguments()) {
            if (arg->hasProperty(property)) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Expression> clone() const override {
        return std::make_unique<ExternalFunctionCall>(fLine, &this->function(),
                                                      this->arguments().clone());
    }

    std::string description() const override {
        std::string result = std::string(this->function().name()) + "(";
        std::string separator;
        for (const std::unique_ptr<Expression>& arg : this->arguments()) {
            result += separator;
            result += arg->description();
            separator = ", ";
        }
        result += ")";
        return result;
    }

private:
    const ExternalFunction& fFunction;
    ExpressionArray fArguments;

    using INHERITED = Expression;
};

}  // namespace SkSL

#endif
