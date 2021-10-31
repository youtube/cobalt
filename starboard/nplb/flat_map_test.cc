// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/common/flat_map.h"

#include <map>
#include <sstream>
#include <string>

#include "starboard/system.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

bool StringPairEquals(const std::pair<std::string, std::string>& a,
                      const std::pair<std::string, std::string>& b) {
  return (a.first == b.first) && (a.second == b.second);
}

bool FlipCoin() {
  return (std::rand() % 2) == 1;
}

int Random(int first_inclusive, int end_exclusive) {
  size_t range = static_cast<size_t>(end_exclusive - first_inclusive);
  size_t rand = 0;
  SbSystemGetRandomData(&rand, sizeof(rand));
  return static_cast<int>(rand % range) + first_inclusive;
}

template <typename MapA_Type, typename MapB_Type>
bool CheckMapEquality(const MapA_Type& map_a, const MapB_Type& map_b) {
  typedef typename MapA_Type::const_iterator map_a_iterator;
  typedef typename MapB_Type::const_iterator map_b_iterator;

  if (map_a.size() != map_b.size()) {
    typedef typename MapA_Type::key_type key_type;
    std::vector<key_type> vector_a;
    std::vector<key_type> vector_b;

    for (map_a_iterator it = map_a.begin(); it != map_a.end(); ++it) {
      vector_a.push_back(it->first);
    }

    for (map_b_iterator it = map_b.begin(); it != map_b.end(); ++it) {
      vector_b.push_back(it->first);
    }

    std::vector<key_type> diff_vector;
    std::set_symmetric_difference(vector_a.begin(), vector_a.end(),
                                  vector_b.begin(), vector_b.end(),
                                  std::back_inserter(diff_vector));

    for (int i = 0; i < diff_vector.size(); ++i) {
      EXPECT_TRUE(false) << "Mismatched key: " << diff_vector[i] << "\n";
    }
    return false;
  }

  map_a_iterator map_a_it = map_a.begin();
  map_b_iterator map_b_it = map_b.begin();

  bool ok = true;

  while (map_a_it != map_a.end()) {
    ok &= (map_a_it->first == map_b_it->first);
    ok &= (map_a_it->second == map_b_it->second);

    EXPECT_EQ(map_a_it->first, map_b_it->first);
    EXPECT_EQ(map_a_it->second, map_b_it->second);

    ++map_a_it;
    ++map_b_it;
  }
  return ok;
}

SbTimeMonotonic GetThreadTimeMonotonicNow() {
#if SB_API_VERSION >= 12 || SB_HAS(TIME_THREAD_NOW)
#if SB_API_VERSION >= 12
  if (SbTimeIsTimeThreadNowSupported())
#endif
    return SbTimeGetMonotonicThreadNow();
#endif
  return SbTimeGetMonotonicNow();
}

// Generic stringification of the input map type. This allows good error
// messages to be printed out.
template <typename MapType>
std::string MapToString(const MapType& map) {
  typedef typename MapType::const_iterator const_iterator;
  std::stringstream ss;
  for (const_iterator it = map.begin(); it != map.end(); ++it) {
    ss << "{" << it->first << "," << it->second << "},\n";
  }
  return ss.str();
}

// Tests FlatMap<int, int> by shadowing operations to an std::map<int, int>
// and checking for equality at every step of the way. This allows "fuzzing"
// the container and checking that it's operation match those from a known
// container.
struct MapTester {
  typedef std::map<int, int>::const_iterator std_map_iterator;
  typedef FlatMap<int, int>::const_iterator flat_map_iterator;

  bool CheckEquality() { return CheckMapEquality(std_map, flat_map); }

  void Insert(int key, int value) {
    typedef std::pair<std_map_iterator, bool> StdMapPair;
    typedef std::pair<flat_map_iterator, bool> FlatMapPair;
    StdMapPair pair_a = std_map.insert(std::make_pair(key, value));
    FlatMapPair pair_b = flat_map.insert(std::make_pair(key, value));

    ASSERT_EQ(pair_a.second, pair_b.second)
        << "Insertion states are mismatched.";

    ASSERT_EQ(pair_a.first->first, pair_b.first->first)
        << "Inserted keys have a mismatch.";

    ASSERT_EQ(pair_a.first->second, pair_b.first->second)
        << "Inserted values have a mismatch.";
    CheckEquality();
  }

  void BulkInsert(const std::vector<std::pair<int, int> >& values) {
    std::map<int, int> old_std_map = std_map;
    FlatMap<int, int> old_flat_map = flat_map;

    std_map.insert(values.begin(), values.end());
    flat_map.insert(values.begin(), values.end());

    if (!CheckEquality()) {
      // Failed so print out something interesting.
      std::string str_old_std_map = MapToString(old_std_map);
      std::string str_std_map = MapToString(std_map);
      std::string str_values = MapToString(values);
      std::string str_flat_map = MapToString(flat_map);

      std::stringstream ss;
      ss << "Original Map:\n" << str_old_std_map << "\n\n";
      ss << "Bulk insert values:\n" << str_values << "\n\n";
      ss << "Resulting map:\n" << str_flat_map << "\n\n";
      ss << "But should have been:\n" << str_std_map << "\n\n";

      SbLogRaw(ss.str().c_str());
    }
  }

  void Erase(int key) {
    std_map.erase(key);
    flat_map.erase(key);
    CheckEquality();
  }

  void BulkErase(const std::vector<int>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
      std_map.erase(values[i]);
      flat_map.erase(values[i]);
    }
    CheckEquality();
  }

  void Clear() {
    std_map.clear();
    flat_map.clear();
    CheckEquality();
  }

  static int RandomKey() { return Random(0, 100); }
  static int RandomValue() { return Random(0, 10000); }

  std::map<int, int> std_map;
  FlatMap<int, int> flat_map;
};
}  // namespace.

////////////////////////////// UNIT TESTS /////////////////////////////////////

TEST(FlatMap, BasicUse) {
  FlatMap<int, int> int_map;
  int_map[4] = 3;
  int_map[3] = 4;

  EXPECT_EQ(2, int_map.size());

  EXPECT_EQ(3, int_map[4]);
  EXPECT_EQ(4, int_map[3]);

  int_map.erase(3);
  EXPECT_EQ(int_map[4], 3);
}

// Tests that a string map correctly can be used with this flat map.
TEST(FlatMap, StringMap) {
  FlatMap<std::string, std::string> string_map;
  string_map["one"] = "value-one";
  string_map["two"] = "value-two";
  string_map["three"] = "value-three";
  string_map["four"] = "value-four";
  string_map["five"] = "value-five";
  EXPECT_EQ(std::string("value-one"), string_map["one"]);
  EXPECT_EQ(std::string("value-two"), string_map["two"]);
  EXPECT_EQ(std::string("value-three"), string_map["three"]);
  EXPECT_EQ(std::string("value-four"), string_map["four"]);
  EXPECT_EQ(std::string("value-five"), string_map["five"]);
}

struct CustomKey {
  CustomKey() : value(0) {}
  explicit CustomKey(int v) : value(v) {}
  int value;
  // Auto-binds to std::less, which is the default comparator of FlatMap
  // as well as
  bool operator<(const CustomKey& other) const { return value < other.value; }
};
TEST(FlatMap, CustomKeyType) {
  FlatMap<CustomKey, int> custom_map;
  custom_map[CustomKey(3)] = 1234;
  EXPECT_EQ(1234, custom_map[CustomKey(3)]);
}

TEST(FlatMap, size) {
  FlatMap<std::string, std::string> flat_map;
  EXPECT_EQ(0, flat_map.size());
  flat_map["one"] = "one-value";
  EXPECT_EQ(1, flat_map.size());
}

TEST(FlatMap, empty) {
  FlatMap<std::string, std::string> flat_map;
  EXPECT_TRUE(flat_map.empty());
  flat_map["one"] = "one-value";
  EXPECT_FALSE(flat_map.empty());
}

TEST(FlatMap, clear) {
  FlatMap<std::string, std::string> flat_map;
  flat_map["one"] = "one-value";
  flat_map.clear();
  EXPECT_TRUE(flat_map.empty());
}

TEST(FlatMap, find) {
  FlatMap<std::string, std::string> flat_map;
  flat_map["one"] = "value-one";
  flat_map["two"] = "value-two";
  flat_map["three"] = "value-three";
  flat_map["four"] = "value-four";
  flat_map["five"] = "value-five";
  EXPECT_EQ(std::string("value-one"), flat_map["one"]);
  EXPECT_EQ(std::string("value-two"), flat_map["two"]);
  EXPECT_EQ(std::string("value-three"), flat_map["three"]);
  EXPECT_EQ(std::string("value-four"), flat_map["four"]);
  EXPECT_EQ(std::string("value-five"), flat_map["five"]);

  FlatMap<std::string, std::string>::const_iterator found_it =
      flat_map.find("three");

  ASSERT_NE(found_it, flat_map.end());
  ASSERT_EQ(std::string("three"), found_it->first);
  ASSERT_EQ(std::string("value-three"), found_it->second);

  found_it = flat_map.find("twenty");

  ASSERT_EQ(found_it, flat_map.end());
}

TEST(FlatMap, swap) {
  FlatMap<int, int> map;
  map[1] = -1;

  FlatMap<int, int> other_map;
  map.swap(other_map);
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(1, other_map.size());
  EXPECT_EQ(-1, other_map[1]);
}

TEST(FlatMap, DefaultAssignmentArrayOperator) {
  FlatMap<int, int> map;
  EXPECT_EQ(0, map[1]);  // key [1] doesn't exist, so should default to 0.
}

TEST(FlatMap, lower_bound) {
  FlatMap<int, int> map;
  map[1] = 1;
  map[3] = 3;
  map[4] = 4;

  FlatMap<int, int>::const_iterator lower_it = map.lower_bound(2);
  ASSERT_TRUE(lower_it != map.end());
  EXPECT_EQ(lower_it->first, 3);

  lower_it = map.lower_bound(3);
  ASSERT_TRUE(lower_it != map.end());
  EXPECT_EQ(lower_it->first, 3);
}

TEST(FlatMap, upper_bound) {
  FlatMap<int, int> map;
  map[1] = 1;
  map[3] = 3;
  map[4] = 4;

  FlatMap<int, int>::const_iterator upper_it = map.upper_bound(2);
  ASSERT_TRUE(upper_it != map.end());
  EXPECT_EQ(upper_it->first, 3);

  upper_it = map.upper_bound(3);
  ASSERT_TRUE(upper_it != map.end());
  EXPECT_EQ(upper_it->first, 4);  // 4 is the next one greater than 3.
}

TEST(FlatMap, equal_range) {
  FlatMap<int, int> map;
  typedef FlatMap<int, int>::iterator iterator;
  map[1] = 1;
  map[3] = 3;
  map[4] = 4;

  // Should not find this.
  std::pair<iterator, iterator> range = map.equal_range(2);
  ASSERT_EQ(range.first, map.end());
  ASSERT_EQ(range.second, map.end());

  // Should find the value.
  range = map.equal_range(3);
  ASSERT_EQ(range.first, map.begin() + 1);
  ASSERT_EQ(range.second, map.begin() + 2);  // exclusive
}

TEST(FlatMap, count) {
  FlatMap<int, int> map;
  typedef FlatMap<int, int>::iterator iterator;
  map[1] = 1;

  EXPECT_EQ(1, map.count(1));
  EXPECT_EQ(0, map.count(4));  // We don't expect this to be found.
}

TEST(FlatMap, OperatorEquals) {
  FlatMap<int, int> map_a;
  FlatMap<int, int> map_b;

  map_a[1] = 1;
  map_b[1] = 1;
  EXPECT_EQ(map_a, map_b);
  map_b[1] = -1;
  EXPECT_NE(map_a, map_b);
  map_b[1] = 1;
  EXPECT_EQ(map_a, map_b);  // Expect equal again.
  map_b[2] = 1;
  EXPECT_NE(map_a, map_b);
}

TEST(FlatMap, Insert) {
  FlatMap<int, int> int_map;

  EXPECT_TRUE(int_map.empty());
  EXPECT_EQ(0, int_map.size());

  int_map.insert(std::make_pair(1, 10));

  EXPECT_FALSE(int_map.empty());
  EXPECT_EQ(1, int_map.size());

  FlatMap<int, int>::iterator found = int_map.find(1);
  EXPECT_EQ(10, found->second);

  std::pair<FlatMap<int, int>::iterator, bool> insert_result =
      int_map.insert(std::make_pair(1, 10));

  EXPECT_FALSE(insert_result.second) << "Insert should have been rejected.";
  EXPECT_EQ(insert_result.first, int_map.begin());

  insert_result = int_map.insert(std::make_pair(0, -10));
  EXPECT_TRUE(insert_result.second) << "Insert should have been succeed.";
  // The new beginning contains the new value.
  EXPECT_EQ(insert_result.first, int_map.begin());
}

TEST(FlatMap, BulkInsertZero) {
  FlatMap<std::string, std::string> flat_string_map;
  flat_string_map["one"] = "value-one";
  flat_string_map["two"] = "value-two";
  flat_string_map["three"] = "value-three";
  flat_string_map["four"] = "value-four";
  flat_string_map["five"] = "value-five";

  std::map<std::string, std::string> string_map;  // empty.
  const size_t num_inserted =
      flat_string_map.insert(string_map.begin(), string_map.end());
  ASSERT_EQ(0, num_inserted);
  EXPECT_EQ(5, flat_string_map.size());
  // According to the sort invariant, "five" is the lowest value key and
  // therefore should be the first element found in the map.
  EXPECT_EQ(std::string("value-five"), flat_string_map.begin()->second);
}

TEST(FlatMap, BulkInsertOne) {
  FlatMap<std::string, std::string> flat_string_map;
  // Reference map verifies for correct behavior.
  std::map<std::string, std::string> reference_map;
  flat_string_map["one"] = "value-one";
  flat_string_map["two"] = "value-two";
  flat_string_map["three"] = "value-three";
  flat_string_map["four"] = "value-four";
  flat_string_map["five"] = "value-five";

  reference_map["one"] = "value-one";
  reference_map["two"] = "value-two";
  reference_map["three"] = "value-three";
  reference_map["four"] = "value-four";
  reference_map["five"] = "value-five";

  std::map<std::string, std::string> string_map;  // empty.
  string_map["six"] = "value-six";
  const size_t num_inserted =
      flat_string_map.insert(string_map.begin(), string_map.end());

  reference_map.insert(string_map.begin(), string_map.end());

  ASSERT_EQ(1, num_inserted);  // "six" = "value-six" was inserted.
  EXPECT_EQ(std::string("value-six"), flat_string_map["six"]);

  CheckMapEquality(flat_string_map, reference_map);
}

TEST(FlatMap, BulkInsertDuplicate) {
  FlatMap<int, int> flat_int_map;
  flat_int_map[1] = 1;

  std::vector<std::pair<int, int> > bulk_entries;
  bulk_entries.push_back(std::pair<int, int>(1, -1));
  flat_int_map.insert(bulk_entries.begin(), bulk_entries.end());

  // Expect that resetting the key [1] => -1 failed because the key already
  // existed.
  EXPECT_EQ(1, flat_int_map[1]);
}

TEST(FlatMap, BulkInsert) {
  std::map<std::string, std::string> string_map;

  string_map["one"] = "value-one";
  string_map["two"] = "value-two";
  string_map["three"] = "value-three";
  string_map["four"] = "value-four";
  string_map["five"] = "value-five";

  FlatMap<std::string, std::string> flat_string_map;
  // Reference map verifies for correct behavior.
  std::map<std::string, std::string> reference_map;
  flat_string_map.insert(string_map.begin(), string_map.end());
  reference_map.insert(string_map.begin(), string_map.end());

  ASSERT_EQ(flat_string_map.size(), string_map.size());
  ASSERT_EQ(reference_map.size(), string_map.size());

  bool is_equal_range = std::equal(string_map.begin(), string_map.end(),
                                   flat_string_map.begin(), StringPairEquals);
  // Now insert again.
  size_t num_inserted =
      flat_string_map.insert(string_map.begin(), string_map.end());
  EXPECT_EQ(0, num_inserted)
      << "No elements should be inserted because they all preexist.";
  reference_map.insert(string_map.begin(), string_map.end());

  // No change in map size.
  ASSERT_EQ(flat_string_map.size(), string_map.size());
  is_equal_range = std::equal(string_map.begin(), string_map.end(),
                              flat_string_map.begin(), StringPairEquals);
  EXPECT_TRUE(is_equal_range);
  CheckMapEquality(flat_string_map, reference_map);
}

TEST(FlatMap, UnsortedInsertWithDuplicates) {
  typedef std::pair<std::string, std::string> StringPair;
  std::vector<StringPair> vector;

  vector.push_back(StringPair("one", "value-one"));
  vector.push_back(StringPair("one", "value-one"));  // Duplicate
  vector.push_back(StringPair("three", "value-three"));
  vector.push_back(StringPair("four", "value-four"));
  vector.push_back(StringPair("five", "value-five"));

  FlatMap<std::string, std::string> flat_string_map;
  flat_string_map.insert(vector.begin(), vector.end());

  // Asserts that the duplicate with key "one" was removed.
  ASSERT_EQ(4, flat_string_map.size());

  std::map<std::string, std::string> string_map;
  string_map["one"] = "value-one";
  string_map["two"] = "value-two";
  string_map["three"] = "value-three";
  string_map["four"] = "value-four";
  string_map["five"] = "value-five";

  const size_t num_inserted =
      flat_string_map.insert(string_map.begin(), string_map.end());
  ASSERT_EQ(1, num_inserted) << "Only one element should have been inserted.";

  bool is_equal_range = std::equal(string_map.begin(), string_map.end(),
                                   flat_string_map.begin(), StringPairEquals);
  ASSERT_TRUE(is_equal_range);
}

TEST(FlatMap, FlatMapDetail_IsPod) {
  EXPECT_TRUE(flat_map_detail::IsPod<bool>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<float>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<int8_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<uint8_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<int16_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<uint16_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<int32_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<uint32_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<int64_t>::value);
  EXPECT_TRUE(flat_map_detail::IsPod<uint64_t>::value);

  EXPECT_TRUE(flat_map_detail::IsPod<CustomKey*>::value);

  EXPECT_FALSE(flat_map_detail::IsPod<std::string>::value);
  EXPECT_FALSE(flat_map_detail::IsPod<std::vector<int> >::value);
}

////////////////////////////// PERFORMANCE TEST ///////////////////////////////

std::vector<int> GenerateRandomIntVector(size_t size_vector,
                                         int min_random,
                                         int max_random) {
  std::vector<int> output;
  for (size_t i = 0; i < size_vector; ++i) {
    output.push_back(Random(min_random, max_random));
  }
  return output;
}

std::vector<std::pair<int, int> > GenerateRandomIntPairVector(
    size_t size_vector,
    int min_random,
    int max_random) {
  std::vector<std::pair<int, int> > output;
  for (size_t i = 0; i < size_vector; ++i) {
    std::pair<int, int> entry(Random(min_random, max_random),
                              Random(min_random, max_random));
    output.push_back(entry);
  }
  return output;
}

template <typename MapIntType>  // FlatMap<int, int> or std::map<int, int>
SbTime PerfTestFind(const MapIntType& map,
                    const std::vector<int>& search_queries_data,
                    size_t query_count) {
  SbThreadYield();  // Stabilizes time
  SbTime start_time = GetThreadTimeMonotonicNow();
  size_t index = 0;
  const size_t n = search_queries_data.size();

  for (size_t i = 0; i < query_count; ++i) {
    if (index == n) {
      index = 0;
    }
    map.find(search_queries_data[index]);
    ++index;
  }
  SbTime delta_time = GetThreadTimeMonotonicNow() - start_time;

  return delta_time;
}

TEST(FlatMap, DISABLED_PerformanceTestFind) {
  std::vector<size_t> test_sizes;
  test_sizes.push_back(5);
  test_sizes.push_back(10);
  test_sizes.push_back(25);
  test_sizes.push_back(50);
  test_sizes.push_back(100);
  test_sizes.push_back(1000);
  test_sizes.push_back(10000);
  test_sizes.push_back(100000);

  std::vector<std::pair<int, int> > insert_data;

  const std::vector<int> query_data =
      GenerateRandomIntVector(1000,     // Number of elements.
                              0,        // Min random value.
                              100000);  // Max random value.

  static const size_t kNumberOfQueries = 10000;

  std::vector<double> speedup_results;

  for (size_t i = 0; i < test_sizes.size(); ++i) {
    const size_t test_size = test_sizes[i];
    insert_data = GenerateRandomIntPairVector(test_size, 0, 100000);

    FlatMap<int, int> flat_int_map(insert_data.begin(), insert_data.end());
    std::map<int, int> std_int_map(insert_data.begin(), insert_data.end());

    SbTime time_flat_int_map =
        PerfTestFind(flat_int_map, query_data, kNumberOfQueries);

    SbTime time_std_int_map =
        PerfTestFind(std_int_map, query_data, kNumberOfQueries);

    double flat_map_speedup = static_cast<double>(time_std_int_map) /
                              static_cast<double>(time_flat_int_map);

    speedup_results.push_back(flat_map_speedup);
  }

  std::stringstream ss;
  ss << "\n";
  ss << "FlatMap<int,int>::find() Performance\n"
     << "NUMBER OF ELEMENTS | SPEED COMPARISON vs std::map\n"
     << "-------------------------------------\n";

  for (size_t i = 0; i < test_sizes.size(); ++i) {
    size_t test_size = test_sizes[i];
    double speedup = speedup_results[i];
    ss.width(18);
    ss << std::right << test_size << " | ";
    ss << std::left << (speedup * 100.0) << "%\n";
  }
  ss << "\n";
  SbLogRaw(ss.str().c_str());
}

///////////////////////////////// FUZZER TEST /////////////////////////////////

// A stochastic test which randomly does insertions and erases and makes sure
// that the two data structures are equal at every step of the way.
TEST(FlatMap, FuzzerTest) {
  static const size_t kNumTestIterations = 1000;
  MapTester map_tester;

  // Seed the random number generator so that any failures are reproducible
  // between runs.
  std::srand(0);

  for (size_t test_loop = 0; test_loop < kNumTestIterations; ++test_loop) {
    const size_t random_1_to_100 = 1 + (std::rand() % 100);

    if (random_1_to_100 > 98) {  // 2% - chance
      // do clear.
      map_tester.Clear();
    } else if (random_1_to_100 > 48) {  // 50% chance
      // Do insert.
      if (FlipCoin()) {
        // Insert one element.
        int key = MapTester::RandomKey();
        int value = MapTester::RandomValue();
        map_tester.Insert(key, value);
      } else {
        // Bulk insert
        const size_t num_values = Random(0, 20);
        std::vector<std::pair<int, int> > values;
        for (size_t i = 0; i < num_values; ++i) {
          int key = MapTester::RandomKey();
          int value = MapTester::RandomValue();
          values.push_back(std::make_pair(key, value));
        }
        map_tester.BulkInsert(values);
      }
    } else {
      // Do erase.
      if (FlipCoin()) {
        // Erase one element.
        int key = Random(0, 100);
        map_tester.Erase(key);
      } else {
        // Erase bulk elements.
        const size_t num_values = Random(0, 20);
        std::vector<int> values;
        for (size_t i = 0; i < num_values; ++i) {
          int key = Random(0, 100);
          values.push_back(key);
        }
        map_tester.BulkErase(values);
      }
    }
    // Now check to make sure maps are still equal.
    map_tester.CheckEquality();
  }
}

}  // namespace nplb
}  // namespace starboard
