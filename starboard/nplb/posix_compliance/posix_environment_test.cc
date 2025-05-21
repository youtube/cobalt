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

#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// This test is placed first to verify the state of 'environ' before
// any other tests (especially those using fixtures with SetUp/TearDown
// that might call setenv/getenv/unsetenv) are run.
TEST(PosixEnvironmentGlobalStateTests, EnvironIsAccessibleAtStartup) {
  ASSERT_NE(nullptr, environ) << "The global 'environ' variable should not be "
                                 "nullptr at program startup";
}

// Assert that environ is declared and accessible, and that its type is
// 'char**'.
static_assert(std::is_same_v<decltype(environ), char**>,
              "'environ' is not declared or is not of type 'char**'");

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
// On non-hermetic builds, getenv() and setenv() are declared "noexcept".
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// Assert that getenv has the signature: char* getenv(const char*)
static_assert(std::is_same_v<decltype(getenv), char*(const char*)>,
              "'getenv' is not declared or does not have the signature "
              "'char* (const char*)'");

// Assert that setenv has the signature:
// int setenv(const char*, const char*, int)
static_assert(
    std::is_same_v<decltype(setenv), int(const char*, const char*, int)>,
    "'setenv' is not declared or does not have the signature "
    "'int (const char*, const char*, int)'");
#endif

class PosixEnvironmentGetenvTests : public ::testing::Test {
 protected:
  static constexpr const char* kVarNameExisting = "POSIX_GETENV_EXISTING";
  static constexpr const char* kVarValueExisting = "hello_getenv";
  static constexpr const char* kVarNameNonExistent =
      "POSIX_GETENV_NON_EXISTENT_VAR_UNIQUE";
  static constexpr const char* kVarNameEmptyVal = "POSIX_GETENV_EMPTY_VAL";

  void SetUp() override {
    TearDown();  // Guarantee to start with an environment without the test
                 // values.
  }

  void TearDown() override {
    unsetenv(kVarNameExisting);
    unsetenv(kVarNameNonExistent);
    unsetenv(kVarNameEmptyVal);
  }
};

TEST_F(PosixEnvironmentGetenvTests, GetExistingVariableReturnsCorrectValue) {
  ASSERT_EQ(0, setenv(kVarNameExisting, kVarValueExisting, 1 /* overwrite */));

  char* retrieved_value = getenv(kVarNameExisting);
  ASSERT_NE(nullptr, retrieved_value)
      << "getenv should return a non-null pointer for an existing variable.";
  ASSERT_STREQ(kVarValueExisting, retrieved_value)
      << "The retrieved value should match the set value.";
}

TEST_F(PosixEnvironmentGetenvTests, GetNonExistentVariableReturnsNullptr) {
  unsetenv(kVarNameNonExistent);  // Ensure it's not set
  char* retrieved_value = getenv(kVarNameNonExistent);
  ASSERT_EQ(nullptr, retrieved_value)
      << "getenv should return nullptr for a non-existent variable.";
}

TEST_F(PosixEnvironmentGetenvTests, GetVariableAfterItsValueIsChangedBySetenv) {
  static constexpr const char* kInitialValue = "initial_val";
  static constexpr const char* kNewValue = "new_val";

  ASSERT_EQ(0, setenv(kVarNameExisting, kInitialValue, 1));
  char* val1 = getenv(kVarNameExisting);
  ASSERT_NE(nullptr, val1);
  ASSERT_STREQ(kInitialValue, val1);

  ASSERT_EQ(0, setenv(kVarNameExisting, kNewValue, 1));
  char* val2 = getenv(kVarNameExisting);
  ASSERT_NE(nullptr, val2);
  ASSERT_STREQ(kNewValue, val2)
      << "getenv should return the new value after setenv updates it.";
}

TEST_F(PosixEnvironmentGetenvTests, GetVariableSetToEmptyString) {
  ASSERT_EQ(0, setenv(kVarNameEmptyVal, "", 1 /* overwrite */));
  char* retrieved_value = getenv(kVarNameEmptyVal);
  ASSERT_NE(nullptr, retrieved_value)
      << "getenv should return non-null for a variable set to an empty string.";
  ASSERT_STREQ("", retrieved_value)
      << "The retrieved value should be an empty string.";
}

class PosixEnvironmentSetenvTests : public ::testing::Test {
 protected:
  static constexpr const char* kVarNew = "POSIX_SETENV_NEW_VAR";
  static constexpr const char* kVarOverwrite = "POSIX_SETENV_OVERWRITE_VAR";
  static constexpr const char* kVarNoOverwrite =
      "POSIX_SETENV_NO_OVERWRITE_VAR";
  static constexpr const char* kVarEmptyValue = "POSIX_SETENV_EMPTY_VALUE_VAR";
  static constexpr const char* kVarNullValue = "POSIX_SETENV_NULL_VALUE_VAR";

  static constexpr const char* kInitialVal = "initial";
  static constexpr const char* kNewVal = "new";

  void SetUp() override {
    TearDown();  // Guarantee to start with an environment without the test
                 // values.
  }

  void TearDown() override {
    unsetenv(kVarNew);
    unsetenv(kVarOverwrite);
    unsetenv(kVarNoOverwrite);
    unsetenv(kVarEmptyValue);
    unsetenv(kVarNullValue);
  }
};

TEST_F(PosixEnvironmentSetenvTests, SetNewVariable) {
  ASSERT_EQ(nullptr, getenv(kVarNew)) << "Variable should not exist initially.";
  ASSERT_EQ(0, setenv(kVarNew, "value1", 1 /* overwrite */))
      << "setenv should succeed for a new variable.";

  char* retrieved = getenv(kVarNew);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("value1", retrieved);
}

TEST_F(PosixEnvironmentSetenvTests, SetNewVariableWithNoOverwriteFlag) {
  ASSERT_EQ(nullptr, getenv(kVarNew)) << "Variable should not exist initially.";
  ASSERT_EQ(0, setenv(kVarNew, "value_no_overwrite_flag", 0 /* no overwrite */))
      << "setenv should succeed for a new variable even with overwrite=0.";

  char* retrieved = getenv(kVarNew);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("value_no_overwrite_flag", retrieved);
}

TEST_F(PosixEnvironmentSetenvTests,
       OverwriteExistingVariableWhenOverwriteIsNonZero) {
  ASSERT_EQ(0, setenv(kVarOverwrite, kInitialVal, 1));
  ASSERT_STREQ(kInitialVal, getenv(kVarOverwrite));

  ASSERT_EQ(0, setenv(kVarOverwrite, kNewVal, 1 /* overwrite */))
      << "setenv should succeed when overwriting.";

  char* retrieved = getenv(kVarOverwrite);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ(kNewVal, retrieved) << "Value should be overwritten.";
}

TEST_F(PosixEnvironmentSetenvTests,
       DoNotOverwriteExistingVariableWhenOverwriteIsZero) {
  ASSERT_EQ(0, setenv(kVarNoOverwrite, kInitialVal, 1));
  ASSERT_STREQ(kInitialVal, getenv(kVarNoOverwrite));

  ASSERT_EQ(0, setenv(kVarNoOverwrite, kNewVal, 0 /* no overwrite */))
      << "setenv should still return 0 even if not overwriting.";

  char* retrieved = getenv(kVarNoOverwrite);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ(kInitialVal, retrieved) << "Value should NOT be overwritten.";
}

TEST_F(PosixEnvironmentSetenvTests, SetValueToEmptyString) {
  ASSERT_EQ(0, setenv(kVarEmptyValue, "non-empty", 1));
  ASSERT_STREQ("non-empty", getenv(kVarEmptyValue));

  ASSERT_EQ(0, setenv(kVarEmptyValue, "", 1 /* overwrite */))
      << "setenv should succeed when setting an empty string value.";

  char* retrieved = getenv(kVarEmptyValue);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("", retrieved) << "Value should be an empty string.";
}

TEST_F(PosixEnvironmentSetenvTests, SetValueToNullptrIsTreatedAsEmptyString) {
  // This tests a common, though not universally POSIX-mandated, behavior
  // where a nullptr value for setenv is treated as an empty string.
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_EQ(0, setenv(kVarNullValue, "non-empty", 1));
  ASSERT_STREQ("non-empty", getenv(kVarNullValue));

  const char* value = nullptr;
  ASSERT_EQ(0, setenv(kVarNullValue, value, 1 /* overwrite */))
      << "setenv should succeed when value is nullptr (treated as empty).";

  char* retrieved = getenv(kVarNullValue);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("", retrieved)
      << "Value should be an empty string when set with nullptr value.";
}

TEST_F(PosixEnvironmentSetenvTests, ErrorWhenNameIsNullptr) {
  errno = 0;
  ASSERT_EQ(-1, setenv(nullptr, "value", 1))
      << "setenv should fail if name is nullptr.";
  ASSERT_EQ(EINVAL, errno) << "errno should be EINVAL for nullptr name.";
}

TEST_F(PosixEnvironmentSetenvTests, ErrorWhenNameIsEmptyString) {
  errno = 0;
  ASSERT_EQ(-1, setenv("", "value", 1))
      << "setenv should fail if name is an empty string.";
  ASSERT_EQ(EINVAL, errno) << "errno should be EINVAL for empty name string.";
}

TEST_F(PosixEnvironmentSetenvTests, ErrorWhenNameContainsEqualsSign) {
  errno = 0;
  ASSERT_EQ(-1, setenv("INVALID=NAME", "value", 1))
      << "setenv should fail if name contains '='.";
  ASSERT_EQ(EINVAL, errno) << "errno should be EINVAL for name containing '='.";
  ASSERT_EQ(nullptr, getenv("INVALID=NAME"))
      << "Variable should not have been set.";
}

// Note on ENOMEM: Insufficient memory to add a new variable or value to the
// environment. This error condition is difficult to reliably trigger in a
// portable unit test.

class PosixEnvironmentEnvironTests : public ::testing::Test {
 protected:
  static constexpr const char* kTestVarName = "POSIX_ENVIRON_TEST_VAR";
  static constexpr const char* kTestVarValue = "environ_rocks";
  static constexpr const char* kTestVarEntry =
      "POSIX_ENVIRON_TEST_VAR=environ_rocks";

  void SetUp() override {
    TearDown();  // Guarantee to start with an environment without the test
                 // values.
  }

  void TearDown() override { unsetenv(kTestVarName); }
};

TEST_F(PosixEnvironmentEnvironTests, EnvironIsNullTerminated) {
  ASSERT_NE(nullptr, environ) << "Precondition: environ should not be nullptr.";
  char** current = environ;
  // Iterate a reasonable number of times to avoid an infinite loop
  // if environ is not NULL-terminated.
  static constexpr int kMaxEnvVarsToCheck = 1000;
  int count = 0;
  while (*current != nullptr && count < kMaxEnvVarsToCheck) {
    current++;
    count++;
  }
  ASSERT_LT(count, kMaxEnvVarsToCheck)
      << "Iteration limit reached, environ might not be NULL-terminated or is "
         "excessively large.";
  ASSERT_EQ(nullptr, *current)
      << "Environ should be terminated by a nullptr pointer.";
}

TEST_F(PosixEnvironmentEnvironTests, EnvironReflectsVariableSetBySetenv) {
  ASSERT_EQ(0, setenv(kTestVarName, kTestVarValue, 1 /* overwrite */));

  ASSERT_NE(nullptr, environ);
  bool found = false;
  for (char** env_ptr = environ; *env_ptr != nullptr; ++env_ptr) {
    if (std::string(*env_ptr) == kTestVarEntry) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found) << "The variable set by setenv (" << kTestVarEntry
                     << ") should be present in environ.";
}

TEST_F(PosixEnvironmentEnvironTests,
       EnvironEntriesAreGenerallyInNameEqualsValueFormat) {
  ASSERT_NE(nullptr, environ);
  if (*environ == nullptr) {
    GTEST_SKIP() << "Skipping test as environment is empty.";
    return;
  }

  // Check the first few entries for the expected "NAME=VALUE" format.
  // Iterate a reasonable number of times to avoid an infinite loop
  // if environ is not NULL-terminated.
  static constexpr int kMaxEnvVarsToCheck = 1000;
  int checked_count = 0;
  for (char** env_ptr = environ;
       *env_ptr != nullptr && checked_count < kMaxEnvVarsToCheck;
       ++env_ptr, ++checked_count) {
    const char* entry = *env_ptr;
    ASSERT_NE(nullptr, strchr(entry, '='))
        << "Environment entry '" << entry
        << "' does not contain an '=' character.";
    ASSERT_NE(entry, strchr(entry, '='))
        << "Environment entry '" << entry
        << "' should not start with an '=' (name should not be empty).";
  }
  ASSERT_GT(checked_count, 0)
      << "Environ seems to be non-empty but no entries were processed.";
}

TEST_F(PosixEnvironmentEnvironTests, EnvironReflectsVariableUnset) {
  ASSERT_EQ(0, setenv(kTestVarName, kTestVarValue, 1));

  bool found_before_unset = false;
  for (char** env_ptr = environ; *env_ptr != nullptr; ++env_ptr) {
    if (std::string(*env_ptr) == kTestVarEntry) {  // Use kTestVarEntry
      found_before_unset = true;
      break;
    }
  }
  ASSERT_TRUE(found_before_unset)
      << "The variable " << kTestVarEntry  // Use kTestVarEntry
      << " should be in environ before unset.";

  ASSERT_EQ(0, unsetenv(kTestVarName));

  // Verify it's no longer in environ. How 'environ' itself is modified
  // by unsetenv is implementation-defined.
  bool found_after_unset = false;
  if (environ != nullptr) {
    for (char** env_ptr = environ; *env_ptr != nullptr; ++env_ptr) {
      // Check if the string starts with "kTestVarName="
      // The length for strncmp should be kTestVarName + '='
      if (strncmp(*env_ptr, kTestVarEntry,  // Use kTestVarEntry for comparison
                  strlen(kTestVarName) + 1) == 0) {
        found_after_unset = true;
        break;
      }
    }
  }
  ASSERT_FALSE(found_after_unset)
      << "Variable " << kTestVarName
      << " should NOT be in environ after unsetenv.";
}

// Note: Direct modification of 'environ' is discouraged and can lead to
// undefined behavior. These tests only observe 'environ' after calls to
// 'setenv' or 'unsetenv'. The 'environ' variable itself is often considered
// legacy; getenv/setenv/unsetenv are preferred.
}  // namespace
}  // namespace nplb
}  // namespace starboard
