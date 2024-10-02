// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

#include <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"

@implementation ElementAppleScript

@synthesize uniqueID = _uniqueID;
@synthesize container = _container;
@synthesize containerProperty = _containerProperty;

// Calling objectSpecifier asks an object to return an object specifier
// record referring to itself. You must call setContainer:property: before
// you can call this method.
- (NSScriptObjectSpecifier*)objectSpecifier {
  return [[[NSUniqueIDSpecifier alloc]
      initWithContainerClassDescription:base::mac::ObjCCast<
                                            NSScriptClassDescription>(
                                            self.container.classDescription)
                     containerSpecifier:self.container.objectSpecifier
                                    key:self.containerProperty
                               uniqueID:self.uniqueID] autorelease];
}

- (void)setContainer:(NSObject*)value property:(NSString*)property {
  self.container = value;
  self.containerProperty = property;
}

- (void)dealloc {
  [_uniqueID release];
  [_container release];
  [_containerProperty release];
  [super dealloc];
}

@end
