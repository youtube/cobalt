//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Compile-time instances of many common TType values. These are looked up
// (statically or dynamically) through the methods defined in the namespace.
//

#ifndef COMPILER_TRANSLATOR_STATIC_TYPE_H_
#define COMPILER_TRANSLATOR_STATIC_TYPE_H_

#include "compiler/translator/Types.h"
#include "nb/cpp14oncpp11.h"

namespace sh
{

namespace StaticType
{

namespace Helpers
{

//
// Generation and static allocation of type mangled name values.
//

// Size of the constexpr-generated mangled name.
// If this value is too small, the compiler will produce errors.
static constexpr size_t kStaticMangledNameLength = TBasicMangledName::mangledNameSize + 1;

// Type which holds the mangled names for constexpr-generated TTypes.
// This simple struct is needed so that a char array can be returned by value.
struct StaticMangledName
{
    // If this array is too small, the compiler will produce errors.
    char name[kStaticMangledNameLength + 1] = {};
};

// Generates a mangled name for a TType given its parameters.
StaticMangledName BuildStaticMangledName(TBasicType basicType,
                                                   TPrecision precision,
                                                   TQualifier qualifier,
                                                   unsigned char primarySize,
                                                   unsigned char secondarySize);

#if !defined(STARBOARD)
// This "variable" contains the mangled names for every constexpr-generated TType.
// If kMangledNameInstance<B, P, Q, PS, SS> is used anywhere (specifally
// in instance, below), this is where the appropriate type will be stored.
template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
static constexpr StaticMangledName kMangledNameInstance =
    BuildStaticMangledName(basicType, precision, qualifier, primarySize, secondarySize);
#else
// Apply a C++11 workaround for variable templates.
// See https://isocpp.org/files/papers/N3651.pdf for details.
template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
struct staticmangledname {
    static CONSTEXPR StaticMangledName kMangledNameInstance;
};

template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
   CONSTEXPR StaticMangledName staticmangledname<basicType, precision, qualifier, primarySize, secondarySize>::kMangledNameInstance =
     BuildStaticMangledName(basicType, precision, qualifier, primarySize, secondarySize);
#endif
//
// Generation and static allocation of TType values.
//

#if !defined(STARBOARD)
// This "variable" contains every constexpr-generated TType.
// If instance<B, P, Q, PS, SS> is used anywhere (specifally
// in Get, below), this is where the appropriate type will be stored.
//
// TODO(crbug.com/981610): This is constexpr but doesn't follow the kConstant naming convention
// because TType has a mutable member that prevents it from being in .data.rel.ro and makes the
// Android Binary Size builder complain when ANGLE is rolled in Chromium.
template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
static constexpr TType instance =
    TType(basicType,
          precision,
          qualifier,
          primarySize,
          secondarySize,
          kMangledNameInstance<basicType, precision, qualifier, primarySize, secondarySize>.name);
#else
// Apply a C++11 workaround for variable templates.
// See https://isocpp.org/files/papers/N3651.pdf for details.
template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
struct ttype {
  static CONSTEXPR TType instance;
};

template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
  CONSTEXPR TType ttype<basicType, precision, qualifier, primarySize, secondarySize>::instance =
    TType(basicType,
          precision,
          qualifier,
          primarySize,
          secondarySize,
          staticmangledname<basicType, precision, qualifier, primarySize, secondarySize>::kMangledNameInstance.name);
#endif

}  // namespace Helpers

//
// Fully-qualified type lookup.
//

template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char primarySize,
          unsigned char secondarySize>
CONSTEXPR const TType *Get()
{
    static_assert(1 <= primarySize && primarySize <= 4, "primarySize out of bounds");
    static_assert(1 <= secondarySize && secondarySize <= 4, "secondarySize out of bounds");
#if !defined(STARBOARD)
    return &Helpers::instance<basicType, precision, qualifier, primarySize, secondarySize>;
#else
    return &Helpers::ttype<basicType, precision, qualifier, primarySize, secondarySize>::instance;
#endif
}

//
// Overloads
//

template <TBasicType basicType, unsigned char primarySize = 1, unsigned char secondarySize = 1>
constexpr const TType *GetBasic()
{
    return Get<basicType, EbpUndefined, EvqGlobal, primarySize, secondarySize>();
}

template <TBasicType basicType,
          TQualifier qualifier,
          unsigned char primarySize   = 1,
          unsigned char secondarySize = 1>
const TType *GetQualified()
{
    return Get<basicType, EbpUndefined, qualifier, primarySize, secondarySize>();
}

// Dynamic lookup methods (convert runtime values to template args)

namespace Helpers
{

// Helper which takes secondarySize statically but primarySize dynamically.
template <TBasicType basicType,
          TPrecision precision,
          TQualifier qualifier,
          unsigned char secondarySize>
CONSTEXPR const TType *GetForVecMatHelper(unsigned char primarySize)
{
    static_assert(basicType == EbtFloat || basicType == EbtInt || basicType == EbtUInt ||
                      basicType == EbtBool,
                  "unsupported basicType");
    switch (primarySize)
    {
        case 1:
            return Get<basicType, precision, qualifier, 1, secondarySize>();
        case 2:
            return Get<basicType, precision, qualifier, 2, secondarySize>();
        case 3:
            return Get<basicType, precision, qualifier, 3, secondarySize>();
        case 4:
            return Get<basicType, precision, qualifier, 4, secondarySize>();
        default:
            UNREACHABLE();
            return GetBasic<EbtVoid>();
    }
}

}  // namespace Helpers

template <TBasicType basicType,
          TPrecision precision = EbpUndefined,
          TQualifier qualifier = EvqGlobal>
CONSTEXPR const TType *GetForVecMat(unsigned char primarySize, unsigned char secondarySize = 1)
{
    static_assert(basicType == EbtFloat || basicType == EbtInt || basicType == EbtUInt ||
                      basicType == EbtBool,
                  "unsupported basicType");
    switch (secondarySize)
    {
        case 1:
            return Helpers::GetForVecMatHelper<basicType, precision, qualifier, 1>(primarySize);
        case 2:
            return Helpers::GetForVecMatHelper<basicType, precision, qualifier, 2>(primarySize);
        case 3:
            return Helpers::GetForVecMatHelper<basicType, precision, qualifier, 3>(primarySize);
        case 4:
            return Helpers::GetForVecMatHelper<basicType, precision, qualifier, 4>(primarySize);
        default:
            UNREACHABLE();
            return GetBasic<EbtVoid>();
    }
}

template <TBasicType basicType, TPrecision precision = EbpUndefined>
CONSTEXPR const TType *GetForVec(TQualifier qualifier, unsigned char size)
{
    switch (qualifier)
    {
        case EvqGlobal:
            return Helpers::GetForVecMatHelper<basicType, precision, EvqGlobal, 1>(size);
        case EvqOut:
            return Helpers::GetForVecMatHelper<basicType, precision, EvqOut, 1>(size);
        default:
            UNREACHABLE();
            return GetBasic<EbtVoid>();
    }
}

}  // namespace StaticType

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_STATIC_TYPE_H_
