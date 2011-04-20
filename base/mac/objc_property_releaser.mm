// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/objc_property_releaser.h"

#import <objc/runtime.h>
#include <stdlib.h>

#include "base/logging.h"

namespace base {
namespace mac {

namespace {

// Returns the name of the instance variable backing the property, if known,
// if the property is marked "retain" or "copy". If the instance variable name
// is not known (perhaps because it was not automatically associated with the
// property by @synthesize) or if the property is not "retain" or "copy",
// returns NULL.
const char* ReleasableInstanceName(objc_property_t property) {
  // TODO(mark): Starting in newer system releases, the Objective-C runtime
  // provides a function to break the property attribute string into
  // individual attributes (property_copyAttributeList), as well as a function
  // to look up the value of a specific attribute
  // (property_copyAttributeValue). When the SDK defining that interface is
  // final, this function should be adapted to walk the attribute list as
  // returned by property_copyAttributeList when that function is available in
  // preference to scanning through the attribute list manually.

  // The format of the string returned by property_getAttributes is documented
  // at
  // http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/ObjCRuntimeGuide/Articles/ocrtPropertyIntrospection.html#//apple_ref/doc/uid/TP40008048-CH101-SW6
  const char* property_attributes = property_getAttributes(property);

  bool releasable = false;
  while (*property_attributes) {
    switch (*property_attributes) {
      // It might seem intelligent to check the type ('T') attribute to verify
      // that it identifies an NSObject-derived type (the attribute value
      // begins with '@'.) This is a bad idea beacuse it fails to identify
      // CFTypeRef-based properties declared as __attribute__((NSObject)),
      // which just show up as pointers to their underlying CFType structs.
      //
      // Quoting
      // http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/ObjectiveC/Chapters/ocProperties.html#//apple_ref/doc/uid/TP30001163-CH17-SW27
      //
      // > In Mac OS X v10.6 and later, you can use the __attribute__ keyword
      // > to specify that a Core Foundation property should be treated like
      // > an Objective-C object for memory management:
      // >   @property(retain) __attribute__((NSObject)) CFDictionaryRef
      // >       myDictionary;
      case 'C':  // copy
      case '&':  // retain
        releasable = true;
        break;
      case 'V':
        // 'V' is specified as the last attribute to occur, so if releasable
        // wasn't set to true already, it won't be set to true now. Since 'V'
        // is last, its value up to the end of the attribute string is the
        // name of the instance variable backing the property.
        if (!releasable || !*++property_attributes) {
          return NULL;
        }
        return property_attributes;
    }
    char previous_char = *property_attributes;
    while (*++property_attributes && previous_char != ',') {
      previous_char = *property_attributes;
    }
  }
  return NULL;
}

}  // namespace

void ObjCPropertyReleaser::Init(id object, Class classy) {
  DCHECK(!object_);
  DCHECK(!class_);
  DCHECK([object isKindOfClass:classy]);

  object_ = object;
  class_ = classy;
}

void ObjCPropertyReleaser::ReleaseProperties() {
  CHECK(object_);
  CHECK(class_);

  unsigned int property_count = 0;
  objc_property_t* properties = class_copyPropertyList(class_, &property_count);

  for (unsigned int property_index = 0;
       property_index < property_count;
       ++property_index) {
    objc_property_t property = properties[property_index];
    const char* instance_name = ReleasableInstanceName(property);
    if (instance_name) {
      id instance_value = nil;
      object_getInstanceVariable(object_, instance_name,
                                 (void**)&instance_value);
      [instance_value release];
    }
  }

  free(properties);

  // Clear object_ and class_ in case this ObjCPropertyReleaser will live on.
  // It's only expected to release the properties it supervises once per Init.
  object_ = nil;
  class_ = nil;
}

}  // namespace mac
}  // namespace base
