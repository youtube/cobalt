//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Compile-time instances of many common TType values. These are looked up
// (statically or dynamically) through the methods defined in the namespace.
//

#include "compiler/translator/StaticType.h"

namespace sh
{

namespace StaticType
{

namespace Helpers
{
StaticMangledName BuildStaticMangledName(TBasicType basicType,
                                                   TPrecision precision,
                                                   TQualifier qualifier,
                                                   unsigned char primarySize,
                                                   unsigned char secondarySize)
{
    StaticMangledName name = {};
    name.name[0]           = TType::GetSizeMangledName(primarySize, secondarySize);
    TBasicMangledName typeName(basicType);
    char *mangledName = typeName.getName();
    static_assert(TBasicMangledName::mangledNameSize == 2, "Mangled name size is not 2");
    name.name[1] = mangledName[0];
    name.name[2] = mangledName[1];
    name.name[3] = '\0';
    return name;
}

}  // namespace Helpers

}  // namespace StaticType

}  // namespace sh
