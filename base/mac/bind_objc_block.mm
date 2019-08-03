// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/bind_objc_block.h"

#include "base/bind.h"
#include "base/memory/scoped_nsobject.h"

namespace {

// Run the block contained in the parameter.
void RunBlock(scoped_nsobject<id> block) {
  void(^extracted_block)() = block.get();
  extracted_block();
}

}  // namespace

namespace base {

base::Closure BindBlock(void(^block)()) {
  return base::Bind(&RunBlock, scoped_nsobject<id>([block copy]));
}

}  // namespace base
