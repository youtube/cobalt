// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MAC_FOUNDATION_UTIL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MAC_FOUNDATION_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>

namespace partition_alloc::internal::base::mac {

// CFCast<>() and CFCastStrict<>() cast a basic CFTypeRef to a more
// specific CoreFoundation type. The compatibility of the passed
// object is found by comparing its opaque type against the
// requested type identifier. If the supplied object is not
// compatible with the requested return type, CFCast<>() returns
// NULL and CFCastStrict<>() will DCHECK. Providing a NULL pointer
// to either variant results in NULL being returned without
// triggering any DCHECK.
//
// Example usage:
// CFNumberRef some_number = base::mac::CFCast<CFNumberRef>(
//     CFArrayGetValueAtIndex(array, index));
//
// CFTypeRef hello = CFSTR("hello world");
// CFStringRef some_string = base::mac::CFCastStrict<CFStringRef>(hello);

template <typename T>
T CFCast(const CFTypeRef& cf_val);

template <typename T>
T CFCastStrict(const CFTypeRef& cf_val);

#define PA_CF_CAST_DECL(TypeCF)                             \
  template <>                                               \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val); \
                                                            \
  template <>                                               \
  TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val)

PA_CF_CAST_DECL(CFArray);
PA_CF_CAST_DECL(CFBag);
PA_CF_CAST_DECL(CFBoolean);
PA_CF_CAST_DECL(CFData);
PA_CF_CAST_DECL(CFDate);
PA_CF_CAST_DECL(CFDictionary);
PA_CF_CAST_DECL(CFNull);
PA_CF_CAST_DECL(CFNumber);
PA_CF_CAST_DECL(CFSet);
PA_CF_CAST_DECL(CFString);
PA_CF_CAST_DECL(CFURL);
PA_CF_CAST_DECL(CFUUID);

#undef PA_CF_CAST_DECL

}  // namespace partition_alloc::internal::base::mac

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MAC_FOUNDATION_UTIL_H_
