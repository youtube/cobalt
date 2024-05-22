/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLSymbolTable.h"

#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLType.h"
#include "src/sksl/ir/SkSLUnresolvedFunction.h"

namespace SkSL {

std::vector<const FunctionDeclaration*> SymbolTable::GetFunctions(const Symbol& s) {
    switch (s.kind()) {
        case Symbol::Kind::kFunctionDeclaration:
            return { &s.as<FunctionDeclaration>() };
        case Symbol::Kind::kUnresolvedFunction:
            return s.as<UnresolvedFunction>().functions();
        default:
            return std::vector<const FunctionDeclaration*>();
    }
}

const Symbol* SymbolTable::operator[](std::string_view name) {
    return this->lookup(fBuiltin ? nullptr : this, MakeSymbolKey(name));
}

const Symbol* SymbolTable::lookup(SymbolTable* writableSymbolTable, const SymbolKey& key) {
    // Symbol-table lookup can cause new UnresolvedFunction nodes to be created; however, we don't
    // want these to end up in built-in root symbol tables (where they will outlive the Program
    // associated with those UnresolvedFunction nodes). `writableSymbolTable` tracks the closest
    // symbol table to the root which is not a built-in.
    if (!fBuiltin) {
        writableSymbolTable = this;
    }
    const Symbol** symbolPPtr = fSymbols.find(key);
    if (!symbolPPtr) {
        if (fParent) {
            return fParent->lookup(writableSymbolTable, key);
        }
        return nullptr;
    }

    const Symbol* symbol = *symbolPPtr;
    if (fParent) {
        auto functions = GetFunctions(*symbol);
        if (functions.size() > 0) {
            bool modified = false;
            const Symbol* previous = fParent->lookup(writableSymbolTable, key);
            if (previous) {
                auto previousFunctions = GetFunctions(*previous);
                for (const FunctionDeclaration* prev : previousFunctions) {
                    bool found = false;
                    for (const FunctionDeclaration* current : functions) {
                        if (current->matches(*prev)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        functions.push_back(prev);
                        modified = true;
                    }
                }
                if (modified) {
                    SkASSERT(functions.size() > 1);
                    return writableSymbolTable
                                   ? writableSymbolTable->takeOwnershipOfSymbol(
                                             std::make_unique<UnresolvedFunction>(functions))
                                   : nullptr;
                }
            }
        }
    }
    return symbol;
}

const std::string* SymbolTable::takeOwnershipOfString(std::string str) {
    fOwnedStrings.push_front(std::move(str));
    // Because fOwnedStrings is a linked list, pointers to elements are stable.
    return &fOwnedStrings.front();
}

void SymbolTable::addWithoutOwnership(const Symbol* symbol) {
    const std::string_view& name = symbol->name();

    const Symbol*& refInSymbolTable = fSymbols[MakeSymbolKey(name)];
    if (refInSymbolTable == nullptr) {
        refInSymbolTable = symbol;
        return;
    }

    if (!symbol->is<FunctionDeclaration>()) {
        fContext.fErrors->error(symbol->fLine, "symbol '" + std::string(name) +
                                               "' was already defined");
        return;
    }

    std::vector<const FunctionDeclaration*> functions;
    if (refInSymbolTable->is<FunctionDeclaration>()) {
        functions = {&refInSymbolTable->as<FunctionDeclaration>(),
                     &symbol->as<FunctionDeclaration>()};

        refInSymbolTable = this->takeOwnershipOfSymbol(
                std::make_unique<UnresolvedFunction>(std::move(functions)));
    } else if (refInSymbolTable->is<UnresolvedFunction>()) {
        functions = refInSymbolTable->as<UnresolvedFunction>().functions();
        functions.push_back(&symbol->as<FunctionDeclaration>());

        refInSymbolTable = this->takeOwnershipOfSymbol(
                std::make_unique<UnresolvedFunction>(std::move(functions)));
    }
}

const Type* SymbolTable::addArrayDimension(const Type* type, int arraySize) {
    if (arraySize != 0) {
        const std::string* arrayName = this->takeOwnershipOfString(type->getArrayName(arraySize));
        type = this->takeOwnershipOfSymbol(Type::MakeArrayType(*arrayName, *type, arraySize));
    }
    return type;
}

}  // namespace SkSL
