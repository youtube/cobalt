// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_MACROS_INTERNAL_H_
#define UI_BASE_METADATA_METADATA_MACROS_INTERNAL_H_

#include <string>
#include <utility>

#include "ui/base/metadata/metadata_types.h"
#include "ui/base/metadata/property_metadata.h"

// Internal Metadata Generation Helpers ---------------------------------------

#define METADATA_CLASS_NAME_INTERNAL(class_name) class_name##_MetaData

// Metadata Accessors ---------------------------------------------------------
#define METADATA_ACCESSORS_INTERNAL(class_name)   \
  static const char kViewClassName[];             \
  const char* GetClassName() const override;      \
  static ui::metadata::ClassMetaData* MetaData(); \
  /* Don't hide non-const base class version. */  \
  using MetaDataProvider::GetClassMetaData;       \
  const ui::metadata::ClassMetaData* GetClassMetaData() const override;

// A version of METADATA_ACCESSORS_INTERNAL for View, the root of the metadata
// hierarchy; here GetClassName() is not declared as an override.
//
// This also introduces the ReinterpretToBaseClass(), which should exist at the
// root of the hierarchy on which the metadata will be attached. This will take
// the given void*, which should be some instance of a subclass of this base
// class, and return a properly typed class_name*. The body of this function
// does a reinterpret_cast<> of the void* to obtain a class_name*. Doing a
// direct static_cast<> from the void* may result in an incorrect
// pointer-to-instance, which may cause a crash. Using this intermediate step of
// reinterpret_cast<> provides more information to the compiler in order to
// obtain the proper result from the static_cast<>. See |AsClass(void* obj)|
// in property_metadata.h for additional info.
#define METADATA_ACCESSORS_INTERNAL_BASE(class_name) \
  static const char kViewClassName[];                \
  virtual const char* GetClassName() const;          \
  static ui::metadata::ClassMetaData* MetaData();    \
  class_name* ReinterpretToBaseClass(void* obj);     \
  /* Don't hide non-const base class version. */     \
  using MetaDataProvider::GetClassMetaData;          \
  const ui::metadata::ClassMetaData* GetClassMetaData() const override;

// Metadata Class -------------------------------------------------------------
#define METADATA_CLASS_INTERNAL(class_name, file, line)              \
  class METADATA_CLASS_NAME_INTERNAL(class_name)                     \
      : public ui::metadata::ClassMetaData {                         \
   public:                                                           \
    using TheClass = class_name;                                     \
    explicit METADATA_CLASS_NAME_INTERNAL(class_name)()              \
        : ClassMetaData(file, line) {                                \
      BuildMetaData();                                               \
    }                                                                \
    METADATA_CLASS_NAME_INTERNAL(class_name)                         \
    (const METADATA_CLASS_NAME_INTERNAL(class_name) &) = delete;     \
    METADATA_CLASS_NAME_INTERNAL(class_name) & operator=(            \
        const METADATA_CLASS_NAME_INTERNAL(class_name) &) = delete;  \
                                                                     \
   private:                                                          \
    friend class class_name;                                         \
    void BuildMetaData();                                            \
    [[maybe_unused]] static ui::metadata::ClassMetaData* meta_data_; \
  }

#define METADATA_PROPERTY_TYPE_INTERNAL(property_type, property_name, ...) \
  ui::metadata::ObjectPropertyMetaData<                                    \
      TheClass, property_type, decltype(&TheClass::Set##property_name),    \
      &TheClass::Set##property_name,                                       \
      decltype(std::declval<TheClass>().Get##property_name()),             \
      &TheClass::Get##property_name, ##__VA_ARGS__>

#define METADATA_READONLY_PROPERTY_TYPE_INTERNAL(property_type, property_name, \
                                                 ...)                          \
  ui::metadata::ObjectPropertyReadOnlyMetaData<                                \
      TheClass, property_type,                                                 \
      decltype(std::declval<TheClass>().Get##property_name()),                 \
      &TheClass::Get##property_name, ##__VA_ARGS__>

#define METADATA_CLASS_PROPERTY_TYPE_INTERNAL(property_type, property_key, \
                                              ...)                         \
  ui::metadata::ClassPropertyMetaData<TheClass, decltype(property_key),    \
                                      property_type, ##__VA_ARGS__>

#define BEGIN_METADATA_INTERNAL(qualified_class_name, metadata_class_name,    \
                                parent_class_name)                            \
  ui::metadata::ClassMetaData*                                                \
      qualified_class_name::metadata_class_name::meta_data_ = nullptr;        \
                                                                              \
  ui::metadata::ClassMetaData* qualified_class_name::MetaData() {             \
    static_assert(                                                            \
        std::is_base_of<parent_class_name, qualified_class_name>::value,      \
        "class not child of parent");                                         \
    if (!qualified_class_name::metadata_class_name::meta_data_) {             \
      qualified_class_name::metadata_class_name::meta_data_ =                 \
          ui::metadata::MakeAndRegisterClassInfo<                             \
              qualified_class_name::metadata_class_name>();                   \
    }                                                                         \
    return qualified_class_name::metadata_class_name::meta_data_;             \
  }                                                                           \
                                                                              \
  const ui::metadata::ClassMetaData* qualified_class_name::GetClassMetaData() \
      const {                                                                 \
    return MetaData();                                                        \
  }                                                                           \
                                                                              \
  const char* qualified_class_name::GetClassName() const {                    \
    return kViewClassName;                                                    \
  }                                                                           \
  const char qualified_class_name::kViewClassName[] = #qualified_class_name;  \
                                                                              \
  void qualified_class_name::metadata_class_name::BuildMetaData() {           \
    SetTypeName(std::string(#qualified_class_name));

// See the comment above on the METADATA_ACCESSORS_INTERNAL_BASE macro for more
// information. NOTE: This function should not be modified to access |this|,
// directly or indirectly. It should only do what is necessary to convert |obj|
// to a properly typed pointer.
#define METADATA_REINTERPRET_BASE_CLASS_INTERNAL(qualified_class_name, \
                                                 metadata_class_name)  \
  qualified_class_name* qualified_class_name::ReinterpretToBaseClass(  \
      void* obj) {                                                     \
    return reinterpret_cast<qualified_class_name*>(obj);               \
  }

#define METADATA_PARENT_CLASS_INTERNAL(parent_class_name) \
  SetParentClassMetaData(parent_class_name::MetaData());

#endif  // UI_BASE_METADATA_METADATA_MACROS_INTERNAL_H_
