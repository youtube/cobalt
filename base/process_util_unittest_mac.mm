// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util_unittest_mac.h"

#import <Cocoa/Cocoa.h>

@interface PsychoticallyBigObjCObject : NSObject
{
  // On 32 bits, the compiler limits Objective C objects to < 2G in size, and on
  // 64 bits, the ObjC2 Runtime Reference says that sizeof(anInstance) is
  // constrained to 32 bits. Keep it < 2G for simplicity.
  int justUnder2Gigs_[(2U * 1024 * 1024 * 1024 - 1) / sizeof(int)];
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
