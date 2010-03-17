// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util_unittest_mac.h"

#import <Cocoa/Cocoa.h>

@interface PsychoticallyBigObjCObject : NSObject
{
#if __LP64__
  // On 64 bits, Objective C objects have no size limit that I can find.
  int tooBigToCount_[0xF000000000000000U / sizeof(int)];
#else
  // On 32 bits, Objective C objects must be < 2G in size.
  int justUnder2Gigs_[(2U * 1024 * 1024 * 1024 - 1) / sizeof(int)];
#endif
}

@end

@implementation PsychoticallyBigObjCObject

@end


namespace base {

void* AllocateViaCFAllocatorSystemDefault(int32 size) {
  return CFAllocatorAllocate(kCFAllocatorSystemDefault, size, 0);
}

void* AllocateViaCFAllocatorMalloc(int32 size) {
  return CFAllocatorAllocate(kCFAllocatorMalloc, size, 0);
}

void* AllocateViaCFAllocatorMallocZone(int32 size) {
  return CFAllocatorAllocate(kCFAllocatorMallocZone, size, 0);
}

void* AllocatePsychoticallyBigObjCObject() {
  return [[PsychoticallyBigObjCObject alloc] init];
}

}  // namespace base
