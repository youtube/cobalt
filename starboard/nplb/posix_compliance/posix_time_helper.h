// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIME_HELPER_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIME_HELPER_H_

#include <gtest/gtest.h>
#include <time.h>

#include <type_traits>

namespace nplb {

// Helper templates to check for the existence of struct tm members
template <typename T, typename = void>
struct has_tm_sec : std::false_type {};
template <typename T>
struct has_tm_sec<T, std::void_t<decltype(T::tm_sec)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_min : std::false_type {};
template <typename T>
struct has_tm_min<T, std::void_t<decltype(T::tm_min)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_hour : std::false_type {};
template <typename T>
struct has_tm_hour<T, std::void_t<decltype(T::tm_hour)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_mday : std::false_type {};
template <typename T>
struct has_tm_mday<T, std::void_t<decltype(T::tm_mday)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_mon : std::false_type {};
template <typename T>
struct has_tm_mon<T, std::void_t<decltype(T::tm_mon)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_year : std::false_type {};
template <typename T>
struct has_tm_year<T, std::void_t<decltype(T::tm_year)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_wday : std::false_type {};
template <typename T>
struct has_tm_wday<T, std::void_t<decltype(T::tm_wday)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_yday : std::false_type {};
template <typename T>
struct has_tm_yday<T, std::void_t<decltype(T::tm_yday)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_isdst : std::false_type {};
template <typename T>
struct has_tm_isdst<T, std::void_t<decltype(T::tm_isdst)>> : std::true_type {};

template <typename T, typename = void>
struct has_tm_gmtoff : std::false_type {};
template <typename T>
struct has_tm_gmtoff<T, std::void_t<decltype(T::tm_gmtoff)>> : std::true_type {
};

template <typename T, typename = void>
struct has_tm_zone : std::false_type {};
template <typename T>
struct has_tm_zone<T, std::void_t<decltype(T::tm_zone)>> : std::true_type {};

// Static assertions for struct tm members
static_assert(has_tm_sec<struct tm>::value,
              "struct tm must have a 'tm_sec' member");
static_assert(std::is_same<decltype(tm::tm_sec), int>::value,
              "The type of 'tm::tm_sec' must be 'int'");

static_assert(has_tm_min<struct tm>::value,
              "struct tm must have a 'tm_min' member");
static_assert(std::is_same<decltype(tm::tm_min), int>::value,
              "The type of 'tm::tm_min' must be 'int'");

static_assert(has_tm_hour<struct tm>::value,
              "struct tm must have a 'tm_hour' member");
static_assert(std::is_same<decltype(tm::tm_hour), int>::value,
              "The type of 'tm::tm_hour' must be 'int'");

static_assert(has_tm_mday<struct tm>::value,
              "struct tm must have a 'tm_mday' member");
static_assert(std::is_same<decltype(tm::tm_mday), int>::value,
              "The type of 'tm::tm_mday' must be 'int'");

static_assert(has_tm_mon<struct tm>::value,
              "struct tm must have a 'tm_mon' member");
static_assert(std::is_same<decltype(tm::tm_mon), int>::value,
              "The type of 'tm::tm_mon' must be 'int'");

static_assert(has_tm_year<struct tm>::value,
              "struct tm must have a 'tm_year' member");
static_assert(std::is_same<decltype(tm::tm_year), int>::value,
              "The type of 'tm::tm_year' must be 'int'");

static_assert(has_tm_wday<struct tm>::value,
              "struct tm must have a 'tm_wday' member");
static_assert(std::is_same<decltype(tm::tm_wday), int>::value,
              "The type of 'tm::tm_wday' must be 'int'");

static_assert(has_tm_yday<struct tm>::value,
              "struct tm must have a 'tm_yday' member");
static_assert(std::is_same<decltype(tm::tm_yday), int>::value,
              "The type of 'tm::tm_yday' must be 'int'");

static_assert(has_tm_isdst<struct tm>::value,
              "struct tm must have a 'tm_isdst' member");
static_assert(std::is_same<decltype(tm::tm_isdst), int>::value,
              "The type of 'tm::tm_isdst' must be 'int'");

static_assert(has_tm_gmtoff<struct tm>::value,
              "struct tm must have a 'tm_gmtoff' member");
static_assert(std::is_same<decltype(tm::tm_gmtoff), long>::value,
              "The type of 'tm::tm_gmtoff' must be 'long'");

static_assert(has_tm_zone<struct tm>::value,
              "struct tm must have a 'tm_zone' member");
static_assert(std::is_same<decltype(tm::tm_zone), const char*>::value,
              "The type of 'tm::tm_zone' must be 'const char*'");

static constexpr time_t kSecondsInDay = 24 * 60 * 60;

// Compare two 'struct tm' using EXPECT_EQ().
void ExpectTmEqual(const struct tm& actual,
                   const struct tm& expected,
                   const std::string& context);

}  // namespace nplb

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIME_HELPER_H_
