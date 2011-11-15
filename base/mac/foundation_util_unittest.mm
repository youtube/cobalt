// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
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

TEST(FoundationUtilTest, ObjCCast) {
  base::mac::ScopedNSAutoreleasePool pool;

  id test_array = [NSArray array];
  id test_array_mutable = [NSMutableArray array];
  id test_data = [NSData data];
  id test_data_mutable = [NSMutableData dataWithCapacity:10];
  id test_date = [NSDate date];
  id test_dict =
      [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:42]
                                  forKey:@"meaning"];
  id test_dict_mutable = [NSMutableDictionary dictionaryWithCapacity:10];
  id test_number = [NSNumber numberWithInt:42];
  id test_null = [NSNull null];
  id test_set = [NSSet setWithObject:@"string object"];
  id test_set_mutable = [NSMutableSet setWithCapacity:10];
  id test_str = [NSString string];
  id test_str_const = @"bonjour";
  id test_str_mutable = [NSMutableString stringWithCapacity:10];

  // Make sure the allocations of NS types are good.
  EXPECT_TRUE(test_array);
  EXPECT_TRUE(test_array_mutable);
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

  // Casting the id correctly provides the same pointer.
  EXPECT_EQ(test_array, base::mac::ObjCCast<NSArray>(test_array));
  EXPECT_EQ(test_array_mutable,
            base::mac::ObjCCast<NSArray>(test_array_mutable));
  EXPECT_EQ(test_data, base::mac::ObjCCast<NSData>(test_data));
  EXPECT_EQ(test_data_mutable,
            base::mac::ObjCCast<NSData>(test_data_mutable));
  EXPECT_EQ(test_date, base::mac::ObjCCast<NSDate>(test_date));
  EXPECT_EQ(test_dict, base::mac::ObjCCast<NSDictionary>(test_dict));
  EXPECT_EQ(test_dict_mutable,
            base::mac::ObjCCast<NSDictionary>(test_dict_mutable));
  EXPECT_EQ(test_number, base::mac::ObjCCast<NSNumber>(test_number));
  EXPECT_EQ(test_null, base::mac::ObjCCast<NSNull>(test_null));
  EXPECT_EQ(test_set, base::mac::ObjCCast<NSSet>(test_set));
  EXPECT_EQ(test_set_mutable, base::mac::ObjCCast<NSSet>(test_set_mutable));
  EXPECT_EQ(test_str, base::mac::ObjCCast<NSString>(test_str));
  EXPECT_EQ(test_str_const, base::mac::ObjCCast<NSString>(test_str_const));
  EXPECT_EQ(test_str_mutable,
            base::mac::ObjCCast<NSString>(test_str_mutable));

  // When given an incorrect ObjC cast, provide nil.
  EXPECT_FALSE(base::mac::ObjCCast<NSString>(test_array));
  EXPECT_FALSE(base::mac::ObjCCast<NSString>(test_array_mutable));
  EXPECT_FALSE(base::mac::ObjCCast<NSString>(test_data));
  EXPECT_FALSE(base::mac::ObjCCast<NSString>(test_data_mutable));
  EXPECT_FALSE(base::mac::ObjCCast<NSSet>(test_date));
  EXPECT_FALSE(base::mac::ObjCCast<NSSet>(test_dict));
  EXPECT_FALSE(base::mac::ObjCCast<NSNumber>(test_dict_mutable));
  EXPECT_FALSE(base::mac::ObjCCast<NSNull>(test_number));
  EXPECT_FALSE(base::mac::ObjCCast<NSDictionary>(test_null));
  EXPECT_FALSE(base::mac::ObjCCast<NSDictionary>(test_set));
  EXPECT_FALSE(base::mac::ObjCCast<NSDate>(test_set_mutable));
  EXPECT_FALSE(base::mac::ObjCCast<NSData>(test_str));
  EXPECT_FALSE(base::mac::ObjCCast<NSData>(test_str_const));
  EXPECT_FALSE(base::mac::ObjCCast<NSArray>(test_str_mutable));

  // Giving a nil provides a nil.
  EXPECT_FALSE(base::mac::ObjCCast<NSArray>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSData>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSDate>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSDictionary>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSNull>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSNumber>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSSet>(nil));
  EXPECT_FALSE(base::mac::ObjCCast<NSString>(nil));

  // ObjCCastStrict: correct cast results in correct pointer being returned.
  EXPECT_EQ(test_array, base::mac::ObjCCastStrict<NSArray>(test_array));
  EXPECT_EQ(test_array_mutable,
            base::mac::ObjCCastStrict<NSArray>(test_array_mutable));
  EXPECT_EQ(test_data, base::mac::ObjCCastStrict<NSData>(test_data));
  EXPECT_EQ(test_data_mutable,
            base::mac::ObjCCastStrict<NSData>(test_data_mutable));
  EXPECT_EQ(test_date, base::mac::ObjCCastStrict<NSDate>(test_date));
  EXPECT_EQ(test_dict, base::mac::ObjCCastStrict<NSDictionary>(test_dict));
  EXPECT_EQ(test_dict_mutable,
            base::mac::ObjCCastStrict<NSDictionary>(test_dict_mutable));
  EXPECT_EQ(test_number, base::mac::ObjCCastStrict<NSNumber>(test_number));
  EXPECT_EQ(test_null, base::mac::ObjCCastStrict<NSNull>(test_null));
  EXPECT_EQ(test_set, base::mac::ObjCCastStrict<NSSet>(test_set));
  EXPECT_EQ(test_set_mutable,
            base::mac::ObjCCastStrict<NSSet>(test_set_mutable));
  EXPECT_EQ(test_str, base::mac::ObjCCastStrict<NSString>(test_str));
  EXPECT_EQ(test_str_const,
            base::mac::ObjCCastStrict<NSString>(test_str_const));
  EXPECT_EQ(test_str_mutable,
            base::mac::ObjCCastStrict<NSString>(test_str_mutable));

  // ObjCCastStrict: Giving a nil provides a nil.
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSArray>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSData>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSDate>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSDictionary>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSNull>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSNumber>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSSet>(nil));
  EXPECT_FALSE(base::mac::ObjCCastStrict<NSString>(nil));
}

TEST(FoundationUtilTest, GetValueFromDictionary) {
  int one = 1, two = 2, three = 3;

  base::mac::ScopedCFTypeRef<CFNumberRef> cf_one(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one));
  base::mac::ScopedCFTypeRef<CFNumberRef> cf_two(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &two));
  base::mac::ScopedCFTypeRef<CFNumberRef> cf_three(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &three));

  CFStringRef keys[] = { CFSTR("one"), CFSTR("two"), CFSTR("three") };
  CFNumberRef values[] = { cf_one, cf_two, cf_three };

  COMPILE_ASSERT(arraysize(keys) == arraysize(values),
                 keys_and_values_arraysizes_are_different);

  base::mac::ScopedCFTypeRef<CFDictionaryRef> test_dict(
      CFDictionaryCreate(kCFAllocatorDefault,
                         reinterpret_cast<const void**>(keys),
                         reinterpret_cast<const void**>(values),
                         arraysize(values),
                         &kCFCopyStringDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));

  // base::mac::GetValueFromDictionary<>(_, _) should produce the correct
  // expected output.
  EXPECT_EQ(values[0],
            base::mac::GetValueFromDictionary<CFNumberRef>(test_dict,
                                                           CFSTR("one")));
  EXPECT_EQ(values[1],
            base::mac::GetValueFromDictionary<CFNumberRef>(test_dict,
                                                           CFSTR("two")));
  EXPECT_EQ(values[2],
            base::mac::GetValueFromDictionary<CFNumberRef>(test_dict,
                                                           CFSTR("three")));

  // Bad input should produce bad output.
  EXPECT_FALSE(base::mac::GetValueFromDictionary<CFNumberRef>(test_dict,
                                                              CFSTR("four")));
  EXPECT_FALSE(base::mac::GetValueFromDictionary<CFStringRef>(test_dict,
                                                              CFSTR("one")));
}
