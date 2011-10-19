// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"

#include "base/mac/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FoundationUtilTest, CFCast) {
  // Build out the CF types to be tested as empty containers.
  base::mac::ScopedCFTypeRef<CFTypeRef> test_array(
      CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_array_mutable(
      CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_bag(
      CFBagCreate(NULL, NULL, 0, &kCFTypeBagCallBacks));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_bag_mutable(
      CFBagCreateMutable(NULL, 0, &kCFTypeBagCallBacks));
  CFTypeRef test_bool = kCFBooleanTrue;
  base::mac::ScopedCFTypeRef<CFTypeRef> test_data(
      CFDataCreate(NULL, NULL, 0));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_data_mutable(
      CFDataCreateMutable(NULL, 0));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_date(
      CFDateCreate(NULL, 0));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_dict(
      CFDictionaryCreate(NULL, NULL, NULL, 0,
                         &kCFCopyStringDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_dict_mutable(
      CFDictionaryCreateMutable(NULL, 0,
                                &kCFCopyStringDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  int int_val = 256;
  base::mac::ScopedCFTypeRef<CFTypeRef> test_number(
      CFNumberCreate(NULL, kCFNumberIntType, &int_val));
  CFTypeRef test_null = kCFNull;
  base::mac::ScopedCFTypeRef<CFTypeRef> test_set(
      CFSetCreate(NULL, NULL, 0, &kCFTypeSetCallBacks));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_set_mutable(
      CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks));
  base::mac::ScopedCFTypeRef<CFTypeRef> test_str(
      CFStringCreateWithBytes(NULL, NULL, 0, kCFStringEncodingASCII,
                              false));
  CFTypeRef test_str_const = CFSTR("hello");
  base::mac::ScopedCFTypeRef<CFTypeRef> test_str_mutable(
      CFStringCreateMutable(NULL, 0));

  // Make sure the allocations of CF types are good.
  EXPECT_TRUE(test_array);
  EXPECT_TRUE(test_array_mutable);
  EXPECT_TRUE(test_bag);
  EXPECT_TRUE(test_bag_mutable);
  EXPECT_TRUE(test_bool);
  EXPECT_TRUE(test_data);
  EXPECT_TRUE(test_data_mutable);
  EXPECT_TRUE(test_date);
  EXPECT_TRUE(test_dict);
  EXPECT_TRUE(test_dict_mutable);
  EXPECT_TRUE(test_number);
  EXPECT_TRUE(test_null);
  EXPECT_TRUE(test_set);
  EXPECT_TRUE(test_set_mutable);
  EXPECT_TRUE(test_str);
  EXPECT_TRUE(test_str_const);
  EXPECT_TRUE(test_str_mutable);

  // Casting the CFTypeRef objects correctly provides the same pointer.
  EXPECT_EQ(test_array, base::mac::CFCast<CFArrayRef>(test_array));
  EXPECT_EQ(test_array_mutable,
            base::mac::CFCast<CFArrayRef>(test_array_mutable));
  EXPECT_EQ(test_bag, base::mac::CFCast<CFBagRef>(test_bag));
  EXPECT_EQ(test_bag_mutable,
            base::mac::CFCast<CFBagRef>(test_bag_mutable));
  EXPECT_EQ(test_bool, base::mac::CFCast<CFBooleanRef>(test_bool));
  EXPECT_EQ(test_data, base::mac::CFCast<CFDataRef>(test_data));
  EXPECT_EQ(test_data_mutable,
            base::mac::CFCast<CFDataRef>(test_data_mutable));
  EXPECT_EQ(test_date, base::mac::CFCast<CFDateRef>(test_date));
  EXPECT_EQ(test_dict, base::mac::CFCast<CFDictionaryRef>(test_dict));
  EXPECT_EQ(test_dict_mutable,
            base::mac::CFCast<CFDictionaryRef>(test_dict_mutable));
  EXPECT_EQ(test_number, base::mac::CFCast<CFNumberRef>(test_number));
  EXPECT_EQ(test_null, base::mac::CFCast<CFNullRef>(test_null));
  EXPECT_EQ(test_set, base::mac::CFCast<CFSetRef>(test_set));
  EXPECT_EQ(test_set_mutable, base::mac::CFCast<CFSetRef>(test_set_mutable));
  EXPECT_EQ(test_str, base::mac::CFCast<CFStringRef>(test_str));
  EXPECT_EQ(test_str_const, base::mac::CFCast<CFStringRef>(test_str_const));
  EXPECT_EQ(test_str_mutable,
            base::mac::CFCast<CFStringRef>(test_str_mutable));

  // When given an incorrect CF cast, provide NULL.
  EXPECT_FALSE(base::mac::CFCast<CFStringRef>(test_array));
  EXPECT_FALSE(base::mac::CFCast<CFStringRef>(test_array_mutable));
  EXPECT_FALSE(base::mac::CFCast<CFStringRef>(test_bag));
  EXPECT_FALSE(base::mac::CFCast<CFSetRef>(test_bag_mutable));
  EXPECT_FALSE(base::mac::CFCast<CFSetRef>(test_bool));
  EXPECT_FALSE(base::mac::CFCast<CFNullRef>(test_data));
  EXPECT_FALSE(base::mac::CFCast<CFDictionaryRef>(test_data_mutable));
  EXPECT_FALSE(base::mac::CFCast<CFDictionaryRef>(test_date));
  EXPECT_FALSE(base::mac::CFCast<CFNumberRef>(test_dict));
  EXPECT_FALSE(base::mac::CFCast<CFDateRef>(test_dict_mutable));
  EXPECT_FALSE(base::mac::CFCast<CFDataRef>(test_number));
  EXPECT_FALSE(base::mac::CFCast<CFDataRef>(test_null));
  EXPECT_FALSE(base::mac::CFCast<CFBooleanRef>(test_set));
  EXPECT_FALSE(base::mac::CFCast<CFBagRef>(test_set_mutable));
  EXPECT_FALSE(base::mac::CFCast<CFBagRef>(test_str));
  EXPECT_FALSE(base::mac::CFCast<CFArrayRef>(test_str_const));
  EXPECT_FALSE(base::mac::CFCast<CFArrayRef>(test_str_mutable));

  // Giving a NULL provides a NULL.
  EXPECT_FALSE(base::mac::CFCast<CFArrayRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFBagRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFBooleanRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFDataRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFDateRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFDictionaryRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFNullRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFNumberRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFSetRef>(NULL));
  EXPECT_FALSE(base::mac::CFCast<CFStringRef>(NULL));

  // CFCastStrict: correct cast results in correct pointer being returned.
  EXPECT_EQ(test_array, base::mac::CFCastStrict<CFArrayRef>(test_array));
  EXPECT_EQ(test_array_mutable,
            base::mac::CFCastStrict<CFArrayRef>(test_array_mutable));
  EXPECT_EQ(test_bag, base::mac::CFCastStrict<CFBagRef>(test_bag));
  EXPECT_EQ(test_bag_mutable,
            base::mac::CFCastStrict<CFBagRef>(test_bag_mutable));
  EXPECT_EQ(test_bool, base::mac::CFCastStrict<CFBooleanRef>(test_bool));
  EXPECT_EQ(test_data, base::mac::CFCastStrict<CFDataRef>(test_data));
  EXPECT_EQ(test_data_mutable,
            base::mac::CFCastStrict<CFDataRef>(test_data_mutable));
  EXPECT_EQ(test_date, base::mac::CFCastStrict<CFDateRef>(test_date));
  EXPECT_EQ(test_dict, base::mac::CFCastStrict<CFDictionaryRef>(test_dict));
  EXPECT_EQ(test_dict_mutable,
            base::mac::CFCastStrict<CFDictionaryRef>(test_dict_mutable));
  EXPECT_EQ(test_number, base::mac::CFCastStrict<CFNumberRef>(test_number));
  EXPECT_EQ(test_null, base::mac::CFCastStrict<CFNullRef>(test_null));
  EXPECT_EQ(test_set, base::mac::CFCastStrict<CFSetRef>(test_set));
  EXPECT_EQ(test_set_mutable,
            base::mac::CFCastStrict<CFSetRef>(test_set_mutable));
  EXPECT_EQ(test_str, base::mac::CFCastStrict<CFStringRef>(test_str));
  EXPECT_EQ(test_str_const,
            base::mac::CFCastStrict<CFStringRef>(test_str_const));
  EXPECT_EQ(test_str_mutable,
            base::mac::CFCastStrict<CFStringRef>(test_str_mutable));

  // CFCastStrict: Giving a NULL provides a NULL.
  EXPECT_FALSE(base::mac::CFCastStrict<CFArrayRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFBagRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFBooleanRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFDataRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFDateRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFDictionaryRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFNullRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFNumberRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFSetRef>(NULL));
  EXPECT_FALSE(base::mac::CFCastStrict<CFStringRef>(NULL));
}
