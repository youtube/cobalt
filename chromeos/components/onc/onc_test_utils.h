// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::onc::test_utils {

// Read the file at |filename| as a string. CHECKs if any error occurs.
std::string ReadTestData(const std::string& filename);

// Read a JSON dictionary from |filename| and return it as a base::Value::Dict.
// CHECKs if any error occurs.
base::Value::Dict ReadTestDictionary(const std::string& filename);

// Read a JSON dictionary from |filename| and return it as a base::Value::List.
// CHECKs if any error occurs.
base::Value::List ReadTestList(const std::string& filename);

// Checks that the pointer |actual| is not NULL but points to a value that
// equals |expected|. The intended use case is:
// EXPECT_TRUE(test_utils::Equals(expected, actual));
::testing::AssertionResult Equals(const base::Value::Dict* expected,
                                  const base::Value::Dict* actual);

}  // namespace chromeos::onc::test_utils

#endif  // CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_
