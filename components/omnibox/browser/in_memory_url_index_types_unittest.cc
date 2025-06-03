// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/in_memory_url_index_types.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;

// Helper function for verifying that the contents of a C++ iterable container
// of ints matches a C array ints.
template <typename T>
bool IntArraysEqual(const size_t* expected,
                    size_t expected_size,
                    const T& actual) {
  if (expected_size != actual.size())
    return false;
  for (size_t i = 0; i < expected_size; ++i)
    if (expected[i] != actual[i])
      return false;
  return true;
}

class InMemoryURLIndexTypesTest : public testing::Test {};

TEST_F(InMemoryURLIndexTypesTest, StaticFunctions) {
  // Test String16VectorFromString16
  std::u16string string_a(u"http://www.google.com/ frammy  the brammy");
  WordStarts actual_starts_a;
  String16Vector string_vec =
      String16VectorFromString16(string_a, &actual_starts_a);
  ASSERT_EQ(7U, string_vec.size());
  // See if we got the words we expected.
  EXPECT_EQ(u"http", string_vec[0]);
  EXPECT_EQ(u"www", string_vec[1]);
  EXPECT_EQ(u"google", string_vec[2]);
  EXPECT_EQ(u"com", string_vec[3]);
  EXPECT_EQ(u"frammy", string_vec[4]);
  EXPECT_EQ(u"the", string_vec[5]);
  EXPECT_EQ(u"brammy", string_vec[6]);
  // Verify the word starts.
  size_t expected_starts_a[] = {0, 7, 11, 18, 23, 31, 35};
  EXPECT_TRUE(IntArraysEqual(expected_starts_a, std::size(expected_starts_a),
                             actual_starts_a));

  std::u16string string_b(
      u" funky%20string-with=@strange   sequences, intended(to exceed)");
  WordStarts actual_starts_b;
  string_vec = String16VectorFromString16(string_b, &actual_starts_b);
  ASSERT_EQ(9U, string_vec.size());
  // Note that we stop collecting words and word starts at kMaxSignificantChars.
  size_t expected_starts_b[] = {1, 7, 9, 16, 22, 32, 43, 52, 55};
  EXPECT_TRUE(IntArraysEqual(expected_starts_b, std::size(expected_starts_b),
                             actual_starts_b));

  std::u16string string_c(u"http://www.google.com/frammy_the_brammy");
  WordStarts actual_starts_c;
  string_vec = String16VectorFromString16(string_c, &actual_starts_c);
  ASSERT_EQ(7U, string_vec.size());
  EXPECT_EQ(u"http", string_vec[0]);
  EXPECT_EQ(u"www", string_vec[1]);
  EXPECT_EQ(u"google", string_vec[2]);
  EXPECT_EQ(u"com", string_vec[3]);
  EXPECT_EQ(u"frammy", string_vec[4]);
  EXPECT_EQ(u"the", string_vec[5]);
  EXPECT_EQ(u"brammy", string_vec[6]);
  size_t expected_starts_c[] = {0, 7, 11, 18, 22, 29, 33};
  EXPECT_TRUE(IntArraysEqual(expected_starts_c, std::size(expected_starts_c),
                             actual_starts_c));

  // Test trailing underscore case. https://crbug.com/1071216
  std::u16string string_d(u"http://www.google.com/frammy_");
  WordStarts actual_starts_d;
  string_vec = String16VectorFromString16(string_d, &actual_starts_d);
  ASSERT_EQ(5U, string_vec.size());
  EXPECT_EQ(u"http", string_vec[0]);
  EXPECT_EQ(u"www", string_vec[1]);
  EXPECT_EQ(u"google", string_vec[2]);
  EXPECT_EQ(u"com", string_vec[3]);
  EXPECT_EQ(u"frammy", string_vec[4]);
  size_t expected_starts_d[] = {0, 7, 11, 18, 22};
  EXPECT_TRUE(IntArraysEqual(expected_starts_d, std::size(expected_starts_d),
                             actual_starts_d));

  // Test String16SetFromString16
  std::u16string string_e(u"http://web.google.com/search Google Web Search");
  WordStarts actual_starts_e;
  String16Set string_set = String16SetFromString16(string_e, &actual_starts_e);
  EXPECT_EQ(5U, string_set.size());
  // See if we got the words we expected.
  EXPECT_TRUE(string_set.find(u"com") != string_set.end());
  EXPECT_TRUE(string_set.find(u"google") != string_set.end());
  EXPECT_TRUE(string_set.find(u"http") != string_set.end());
  EXPECT_TRUE(string_set.find(u"search") != string_set.end());
  EXPECT_TRUE(string_set.find(u"web") != string_set.end());
  size_t expected_starts_e[] = {0, 7, 11, 18, 22, 29, 36, 40};
  EXPECT_TRUE(IntArraysEqual(expected_starts_e, std::size(expected_starts_e),
                             actual_starts_e));

  // Test SortMatches and DeoverlapMatches.
  TermMatches matches_e;
  matches_e.push_back(TermMatch(1, 13, 10));
  matches_e.push_back(TermMatch(2, 23, 10));
  matches_e.push_back(TermMatch(3, 3, 10));
  matches_e.push_back(TermMatch(4, 40, 5));
  TermMatches matches_f = DeoverlapMatches(SortMatches(matches_e));
  // Nothing should have been eliminated.
  EXPECT_EQ(matches_e.size(), matches_f.size());
  // The order should now be 3, 1, 2, 4.
  EXPECT_EQ(3, matches_f[0].term_num);
  EXPECT_EQ(1, matches_f[1].term_num);
  EXPECT_EQ(2, matches_f[2].term_num);
  EXPECT_EQ(4, matches_f[3].term_num);
  matches_e.push_back(TermMatch(5, 18, 10));
  matches_e.push_back(TermMatch(6, 38, 5));
  matches_f = DeoverlapMatches(SortMatches(matches_e));
  // Two matches should have been eliminated.
  EXPECT_EQ(matches_e.size() - 2, matches_f.size());
  // The order should now be 3, 1, 2, 6.
  EXPECT_EQ(3, matches_f[0].term_num);
  EXPECT_EQ(1, matches_f[1].term_num);
  EXPECT_EQ(2, matches_f[2].term_num);
  EXPECT_EQ(6, matches_f[3].term_num);

  // Test MatchTermInString
  TermMatches matches_g =
      MatchTermInString(u"x", u"axbxcxdxex fxgx/hxixjx.kx", 123);
  const size_t expected_offsets[] = {1, 3, 5, 7, 9, 12, 14, 17, 19, 21, 24};
  ASSERT_EQ(std::size(expected_offsets), matches_g.size());
  for (size_t i = 0; i < std::size(expected_offsets); ++i)
    EXPECT_EQ(expected_offsets[i], matches_g[i].offset);
}

TEST_F(InMemoryURLIndexTypesTest, OffsetsAndTermMatches) {
  // Test OffsetsFromTermMatches
  TermMatches matches_a;
  matches_a.push_back(TermMatch(1, 1, 2));
  matches_a.push_back(TermMatch(2, 4, 3));
  matches_a.push_back(TermMatch(3, 9, 1));
  matches_a.push_back(TermMatch(3, 10, 1));
  matches_a.push_back(TermMatch(4, 14, 5));
  std::vector<size_t> offsets = OffsetsFromTermMatches(matches_a);
  const size_t expected_offsets_a[] = {1, 3, 4, 7, 9, 10, 10, 11, 14, 19};
  ASSERT_EQ(offsets.size(), std::size(expected_offsets_a));
  for (size_t i = 0; i < offsets.size(); ++i)
    EXPECT_EQ(expected_offsets_a[i], offsets[i]);

  // Test ReplaceOffsetsInTermMatches
  offsets[4] = std::u16string::npos;  // offset of third term
  TermMatches matches_b = ReplaceOffsetsInTermMatches(matches_a, offsets);
  const size_t expected_offsets_b[] = {1, 4, 10, 14};
  ASSERT_EQ(std::size(expected_offsets_b), matches_b.size());
  for (size_t i = 0; i < matches_b.size(); ++i)
    EXPECT_EQ(expected_offsets_b[i], matches_b[i].offset);
}
