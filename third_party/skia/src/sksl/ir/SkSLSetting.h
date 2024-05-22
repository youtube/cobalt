/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_SETTING
#define SKSL_SETTING

#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLExpression.h"

namespace SkSL {

/**
 * Represents a compile-time constant setting, such as sk_Caps.fbFetchSupport. These IRNodes should
 * only exist in a dehydrated module. These nodes are replaced with the value of the setting during
 * rehydration or compilation (i.e., whenever fReplaceSettings is true).
 */
class Setting final : public Expression {
public:
    inline static constexpr Kind kExpressionKind = Kind::kSetting;

    Setting(int line, std::string_view name, const Type* type)
        : INHERITED(line, kExpressionKind, type)
        , fName(std::move(name)) {}

    // Creates an SkSL setting expression if `fReplaceSettings` is false, or the current value of
    // the setting when it is true. Reports errors via the ErrorReporter.
    // (There's no failsafe Make equivalent, because there really isn't a good fallback expression
    // to produce when the `name` lookup fails. We wouldn't even know the expected type.)
    static std::unique_ptr<Expression> Convert(const Context& context, int line,
                                               const std::string_view& name);

    std::unique_ptr<Expression> clone() const override {
        return std::make_unique<Setting>(fLine, this->name(), &this->type());
    }

    const std::string_view& name() const {
        return fName;
    }

    std::string description() const override {
        return std::string(this->name());
    }

    bool hasProperty(Property property) const override {
        return false;
    }

private:
    std::string_view fName;

    using INHERITED = Expression;
};

}  // namespace SkSL

#endif
