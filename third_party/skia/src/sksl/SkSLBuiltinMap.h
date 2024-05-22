/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_BUILTINMAP
#define SKSL_BUILTINMAP

#include "include/private/SkSLString.h"

#include <memory>
#include <unordered_map>

namespace SkSL {

class ProgramElement;

/**
 * Represents the builtin elements in the Context.
 */
class BuiltinMap {
public:
    BuiltinMap(BuiltinMap* parent) : fParent(parent) {}

    void insertOrDie(std::string key, std::unique_ptr<ProgramElement> element);

    const ProgramElement* find(const std::string& key);

    const ProgramElement* findAndInclude(const std::string& key);

    void resetAlreadyIncluded();

private:
    struct BuiltinElement {
        std::unique_ptr<ProgramElement> fElement;
        bool fAlreadyIncluded = false;
    };

    std::unordered_map<std::string, BuiltinElement> fElements;
    BuiltinMap* fParent = nullptr;
};

} // namespace SkSL

#endif
