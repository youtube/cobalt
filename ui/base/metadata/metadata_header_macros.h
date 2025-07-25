// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_HEADER_MACROS_H_
#define UI_BASE_METADATA_METADATA_HEADER_MACROS_H_

#include "ui/base/metadata/metadata_macros_internal.h"
#include "ui/base/metadata/metadata_utils.h"

// Generate Metadata's accessor functions and internal class declaration.
// This should be used in a header file of the View class or its subclasses.
#define _METADATA_HEADER1(class_name)     \
  METADATA_ACCESSORS_INTERNAL(class_name) \
  METADATA_CLASS_INTERNAL(class_name, __FILE__, __LINE__)

#define _METADATA_HEADER2(new_class_name, ancestor_class_name)        \
  static_assert(ui::metadata::kHasClassMetadata<ancestor_class_name>, \
                #ancestor_class_name                                  \
                " doesn't implement metadata. Make "                  \
                "sure class publicly calls METADATA_HEADER in the "   \
                "declaration.");                                      \
                                                                      \
 public:                                                              \
  using kAncestorClass = ancestor_class_name;                         \
  _METADATA_HEADER1(new_class_name);                                  \
                                                                      \
 private:  // NOLINTNEXTLINE

#define _GET_MDH_MACRO_NAME(_1, _2, NAME, ...) NAME

#define METADATA_HEADER(class_name, ...)                            \
  _GET_MDH_MACRO_NAME(class_name, ##__VA_ARGS__, _METADATA_HEADER2, \
                      _METADATA_HEADER1)                            \
  (class_name, ##__VA_ARGS__)

// A version of METADATA_HEADER for View, the root of the metadata hierarchy.
// Here METADATA_ACCESSORS_INTERNAL_BASE is called.
#define METADATA_HEADER_BASE(class_name)       \
  METADATA_ACCESSORS_INTERNAL_BASE(class_name) \
  METADATA_CLASS_INTERNAL(class_name, __FILE__, __LINE__)

#endif  // UI_BASE_METADATA_METADATA_HEADER_MACROS_H_
