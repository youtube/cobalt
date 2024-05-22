/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLSetting.h"

#include "include/sksl/SkSLErrorReporter.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/ir/SkSLLiteral.h"
#include "src/sksl/ir/SkSLVariableReference.h"

namespace SkSL {

// Helper classes for converting caps fields to Expressions and Types in the CapsLookupTable.
namespace {

class CapsLookupMethod {
public:
    virtual ~CapsLookupMethod() {}
    virtual const Type* type(const Context& context) const = 0;
    virtual std::unique_ptr<Expression> value(const Context& context) const = 0;
};

class BoolCapsLookup : public CapsLookupMethod {
public:
    using CapsFn = bool (ShaderCaps::*)() const;

    BoolCapsLookup(const CapsFn& fn) : fGetCap(fn) {}

    const Type* type(const Context& context) const override {
        return context.fTypes.fBool.get();
    }
    std::unique_ptr<Expression> value(const Context& context) const override {
        return Literal::MakeBool(context, /*line=*/-1, (context.fCaps.*fGetCap)());
    }

private:
    CapsFn fGetCap;
};

class IntCapsLookup : public CapsLookupMethod {
public:
    using CapsFn = int (ShaderCaps::*)() const;

    IntCapsLookup(const CapsFn& fn) : fGetCap(fn) {}

    const Type* type(const Context& context) const override {
        return context.fTypes.fInt.get();
    }
    std::unique_ptr<Expression> value(const Context& context) const override {
        return Literal::MakeInt(context, /*line=*/-1, (context.fCaps.*fGetCap)());
    }

private:
    CapsFn fGetCap;
};

class CapsLookupTable {
public:
    using Pair = std::pair<const char*, CapsLookupMethod*>;

    CapsLookupTable(std::initializer_list<Pair> capsLookups) {
        for (auto& entry : capsLookups) {
            fMap[entry.first] = std::unique_ptr<CapsLookupMethod>(entry.second);
        }
    }

    const CapsLookupMethod* lookup(std::string_view name) const {
        auto iter = fMap.find(name);
        return (iter != fMap.end()) ? iter->second.get() : nullptr;
    }

private:
    std::unordered_map<std::string_view, std::unique_ptr<CapsLookupMethod>> fMap;
};

static const CapsLookupTable& caps_lookup_table() {
    // Create a lookup table that converts strings into the equivalent ShaderCaps methods.
    static CapsLookupTable* sCapsLookupTable = new CapsLookupTable({
    #define CAP(T, name) CapsLookupTable::Pair{#name, new T##CapsLookup{&ShaderCaps::name}}
        CAP(Bool, fbFetchSupport),
        CAP(Bool, fbFetchNeedsCustomOutput),
        CAP(Bool, flatInterpolationSupport),
        CAP(Bool, noperspectiveInterpolationSupport),
        CAP(Bool, externalTextureSupport),
        CAP(Bool, mustEnableAdvBlendEqs),
        CAP(Bool, mustDeclareFragmentShaderOutput),
        CAP(Bool, mustDoOpBetweenFloorAndAbs),
        CAP(Bool, mustGuardDivisionEvenAfterExplicitZeroCheck),
        CAP(Bool, atan2ImplementedAsAtanYOverX),
        CAP(Bool, canUseAnyFunctionInShader),
        CAP(Bool, floatIs32Bits),
        CAP(Bool, integerSupport),
        CAP(Bool, builtinFMASupport),
        CAP(Bool, builtinDeterminantSupport),
        CAP(Bool, rewriteMatrixVectorMultiply),
    #undef CAP
    });
    return *sCapsLookupTable;
}

}  // namespace

static const Type* get_type(const Context& context, int line, std::string_view name) {
    if (const CapsLookupMethod* caps = caps_lookup_table().lookup(name)) {
        return caps->type(context);
    }

    context.fErrors->error(line, "unknown capability flag '" + std::string(name) + "'");
    return nullptr;
}

static std::unique_ptr<Expression> get_value(const Context& context, int line,
                                             const std::string_view& name) {
    if (const CapsLookupMethod* caps = caps_lookup_table().lookup(name)) {
        return caps->value(context);
    }

    context.fErrors->error(line, "unknown capability flag '" + std::string(name) + "'");
    return nullptr;
}

std::unique_ptr<Expression> Setting::Convert(const Context& context, int line,
                                             const std::string_view& name) {
    SkASSERT(context.fConfig);

    if (context.fConfig->fSettings.fReplaceSettings) {
        // Insert the settings value directly into the IR.
        return get_value(context, line, name);
    }

    // Generate a Setting IRNode.
    const Type* type = get_type(context, line, name);
    return type ? std::make_unique<Setting>(line, name, type) : nullptr;
}

}  // namespace SkSL
