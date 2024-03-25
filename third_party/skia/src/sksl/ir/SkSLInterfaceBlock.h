/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_INTERFACEBLOCK
#define SKSL_INTERFACEBLOCK

#include <memory>
#include <string_view>

#include "include/private/SkSLProgramElement.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"

namespace SkSL {

/**
 * An interface block, as in:
 *
 * out sk_PerVertex {
 *   layout(builtin=0) float4 sk_Position;
 *   layout(builtin=1) float sk_PointSize;
 * };
 *
 * At the IR level, this is represented by a single variable of struct type.
 */
class InterfaceBlock final : public ProgramElement {
public:
    inline static constexpr Kind kProgramElementKind = Kind::kInterfaceBlock;

    InterfaceBlock(int line,
                   const Variable& var,
                   std::string_view typeName,
                   std::string_view instanceName,
                   int arraySize,
                   std::shared_ptr<SymbolTable> typeOwner)
            : INHERITED(line, kProgramElementKind)
            , fVariable(var)
            , fTypeName(typeName)
            , fInstanceName(instanceName)
            , fArraySize(arraySize)
            , fTypeOwner(std::move(typeOwner)) {
        SkASSERT(fVariable.type().isInterfaceBlock() ||
                 (fVariable.type().isArray() &&
                  fVariable.type().componentType().isInterfaceBlock()));
    }

    const Variable& variable() const {
        return fVariable;
    }

    std::string_view typeName() const {
        return fTypeName;
    }

    std::string_view instanceName() const {
        return fInstanceName;
    }

    const std::shared_ptr<SymbolTable>& typeOwner() const {
        return fTypeOwner;
    }

    int arraySize() const {
        return fArraySize;
    }

    std::unique_ptr<ProgramElement> clone() const override {
        return std::make_unique<InterfaceBlock>(fLine, this->variable(), this->typeName(),
                                                this->instanceName(), this->arraySize(),
                                                SymbolTable::WrapIfBuiltin(this->typeOwner()));
    }

    std::string description() const override {
        std::string result = this->variable().modifiers().description() +
                             std::string(this->typeName()) + " {\n";
        const Type* structType = &this->variable().type();
        if (structType->isArray()) {
            structType = &structType->componentType();
        }
        for (const auto& f : structType->fields()) {
            result += f.description() + "\n";
        }
        result += "}";
        if (!this->instanceName().empty()) {
            result += " " + std::string(this->instanceName());
            if (this->arraySize() > 0) {
                String::appendf(&result, "[%d]", this->arraySize());
            }
        }
        return result + ";";
    }

private:
    const Variable& fVariable;
    std::string_view fTypeName;
    std::string_view fInstanceName;
    int fArraySize;
    std::shared_ptr<SymbolTable> fTypeOwner;

    using INHERITED = ProgramElement;
};

}  // namespace SkSL

#endif
