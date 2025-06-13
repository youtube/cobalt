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

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iterator>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Note: Direct modification of 'environ' is discouraged and can lead to
// undefined behavior. These tests only observe 'environ' after calls to
// 'setenv' or 'unsetenv'. The 'environ' variable itself is often considered
// legacy; getenv/setenv/unsetenv are preferred.

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

// Helper struct and operator to log environment differences.
struct EnvironmentDifference {
  const std::vector<std::string>& start_environ;
  const std::vector<std::string>& end_environ;
};

std::ostream& operator<<(std::ostream& os,
                         const EnvironmentDifference& env_diff) {
  std::vector<std::string> added;
  std::vector<std::string> removed;

  std::set_difference(env_diff.end_environ.begin(), env_diff.end_environ.end(),
                      env_diff.start_environ.begin(),
                      env_diff.start_environ.end(), std::back_inserter(added));

  std::set_difference(env_diff.start_environ.begin(),
                      env_diff.start_environ.end(),
                      env_diff.end_environ.begin(), env_diff.end_environ.end(),
                      std::back_inserter(removed));

  if (!added.empty()) {
    os << "\n  Entries Added:";
    for (const auto& var : added) {
      os << "\n    - " << var;
    }
  }

  if (!removed.empty()) {
    os << "\n  Entries Removed:";
    for (const auto& var : removed) {
      os << "\n    - " << var;
    }
  }

  return os;
}

// This test fixture base class is designed to be inherited by the other test
// harnesses. It captures a snapshot of the environment variables
// at the beginning of SetUp and verifies that the environment is identical
// at the end of TearDown. This ensures that tests do not have side effects
// on the process environment.
class EnvironmentTestBase : public ::testing::Test {
 public:
  EnvironmentTestBase() {
    // This runs BEFORE any SetUp() method.
    // We snapshot the initial environment state.
    for (char** env = environ; *env != nullptr; ++env) {
      start_environ_.push_back(*env);
    }
    std::sort(start_environ_.begin(), start_environ_.end());
  }

  ~EnvironmentTestBase() override {
    // This runs AFTER any TearDown() method.
    // We take a final snapshot and verify it against the initial one.
    if (!HasFatalFailure()) {
      std::vector<std::string> end_environ;
      for (char** env = environ; *env != nullptr; ++env) {
        end_environ.push_back(*env);
      }
      std::sort(end_environ.begin(), end_environ.end());

      EXPECT_EQ(start_environ_, end_environ)
          << "The environment was modified during the test."
          << EnvironmentDifference{start_environ_, end_environ};
    }
  }

 private:
  std::vector<std::string> start_environ_;
};
class PosixEnvironmentGetenvTests : public EnvironmentTestBase {
 protected:
  static constexpr const char* kVarNameExisting = "POSIX_GETENV_EXISTING";
  static constexpr const char* kVarValueExisting = "hello_getenv";
  static constexpr const char* kVarNameNonExistent =
      "POSIX_GETENV_NON_EXISTENT_VAR_UNIQUE";
  static constexpr const char* kVarNameEmptyVal = "POSIX_GETENV_EMPTY_VAL";

  void UnsetVariables() {
    unsetenv(kVarNameExisting);
    unsetenv(kVarNameNonExistent);
    unsetenv(kVarNameEmptyVal);
  }

  void SetUp() override { UnsetVariables(); }
  void TearDown() override { UnsetVariables(); }
};

TEST_F(PosixEnvironmentGetenvTests, GetExistingVariableReturnsCorrectValue) {
  ASSERT_EQ(0, setenv(kVarNameExisting, kVarValueExisting, /*overwrite=*/1));

  char* retrieved_value = getenv(kVarNameExisting);
  ASSERT_NE(nullptr, retrieved_value)
      << "getenv should return a non-null pointer for an existing variable.";
  ASSERT_STREQ(kVarValueExisting, retrieved_value)
      << "The retrieved value should match the set value.";
}

TEST_F(PosixEnvironmentGetenvTests, GetNonExistentVariableReturnsNullptr) {
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
  ASSERT_EQ(0, setenv(kVarNameEmptyVal, "", /*overwrite=*/1));
  char* retrieved_value = getenv(kVarNameEmptyVal);
  ASSERT_NE(nullptr, retrieved_value)
      << "getenv should return non-null for a variable set to an empty string.";
  ASSERT_STREQ("", retrieved_value)
      << "The retrieved value should be an empty string.";
}

class PosixEnvironmentSetenvTests : public EnvironmentTestBase {
 protected:
  static constexpr const char* kVarNew = "POSIX_SETENV_NEW_VAR";
  static constexpr const char* kVarOverwrite = "POSIX_SETENV_OVERWRITE_VAR";
  static constexpr const char* kVarNoOverwrite =
      "POSIX_SETENV_NO_OVERWRITE_VAR";
  static constexpr const char* kVarEmptyValue = "POSIX_SETENV_EMPTY_VALUE_VAR";
  static constexpr const char* kVarNullValue = "POSIX_SETENV_NULL_VALUE_VAR";

  static constexpr const char* kInitialVal = "initial";
  static constexpr const char* kNewVal = "new";

  void UnsetVariables() {
    unsetenv(kVarNew);
    unsetenv(kVarOverwrite);
    unsetenv(kVarNoOverwrite);
    unsetenv(kVarEmptyValue);
    unsetenv(kVarNullValue);
  }

  void SetUp() override { UnsetVariables(); }
  void TearDown() override { UnsetVariables(); }
};

TEST_F(PosixEnvironmentSetenvTests, SetNewVariable) {
  ASSERT_EQ(0, setenv(kVarNew, "value1", /*overwrite=*/1))
      << "setenv should succeed for a new variable.";

  char* retrieved = getenv(kVarNew);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("value1", retrieved);
}

TEST_F(PosixEnvironmentSetenvTests, SetNewVariableWithNoOverwriteFlag) {
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

  ASSERT_EQ(0, setenv(kVarOverwrite, kNewVal, /*overwrite=*/1))
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
  ASSERT_EQ(0, setenv(kVarEmptyValue, "", /*overwrite=*/1))
      << "setenv should succeed when setting an empty string value.";

  char* retrieved = getenv(kVarEmptyValue);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("", retrieved) << "Value should be an empty string.";
}

TEST_F(PosixEnvironmentSetenvTests, SetNonEmptyValueToEmptyString) {
  ASSERT_EQ(0, setenv(kVarEmptyValue, "non-empty", 1));
  ASSERT_STREQ("non-empty", getenv(kVarEmptyValue));

  ASSERT_EQ(0, setenv(kVarEmptyValue, "", /*overwrite=*/1))
      << "setenv should succeed when setting an empty string value.";

  char* retrieved = getenv(kVarEmptyValue);
  ASSERT_NE(nullptr, retrieved);
  ASSERT_STREQ("", retrieved) << "Value should be an empty string.";
}

TEST_F(PosixEnvironmentSetenvTests, SetValueToNullptrIsTreatedAsEmptyString) {
  // This tests a common, though not universally POSIX-mandated, behavior
  // where a nullptr value for setenv is treated as an empty string.
  // TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  // glibc crashes when setenv is called this way, which is not preferred
  // behavior for our application.
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_EQ(0, setenv(kVarNullValue, "non-empty", 1));
  ASSERT_STREQ("non-empty", getenv(kVarNullValue));

  const char* value = nullptr;
  ASSERT_EQ(0, setenv(kVarNullValue, value, /*overwrite=*/1))
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

class PosixEnvironmentEnvironTests : public EnvironmentTestBase {
 protected:
  static constexpr const char* kTestVarName = "POSIX_ENVIRON_TEST_VAR";
  static constexpr const char* kTestVarValue = "environ_rocks";
  static constexpr const char* kTestVarEntry =
      "POSIX_ENVIRON_TEST_VAR=environ_rocks";
  static constexpr const char* kVarEmptyValue = "POSIX_SETENV_EMPTY_VALUE_VAR";
  static constexpr const char* kVarNullValue = "POSIX_SETENV_NULL_VALUE_VAR";

  void UnsetVariables() {
    unsetenv(kTestVarName);
    unsetenv(kVarEmptyValue);
    unsetenv(kVarNullValue);
  }

  void SetUp() override { UnsetVariables(); }
  void TearDown() override { UnsetVariables(); }
};

TEST_F(PosixEnvironmentEnvironTests, EnvironIsNullTerminated) {
  ASSERT_NE(nullptr, environ) << "Precondition: environ should not be nullptr.";
  char** current = environ;
  // Iterate a reasonable number of times to avoid an infinite loop
  // if environ is not NULL-terminated.

  // Since this tests a hermetically implemented environment, it is completely
  // under control of the application code. It is extremely
  // unlikely that we will need more than kMaxEnvVarsToCheck environment
  // variables.

  // The only environment variables that can be in there during this test are
  // ones added by our code before this test runs (e.g. with global static
  // initializers, or at the test main entry point, etc).
  static constexpr int kMaxEnvVarsToCheck = 1'000'000;
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
  ASSERT_EQ(0, setenv(kTestVarName, kTestVarValue, /*overwrite=*/1));

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

  // Set some values to be tested.
  ASSERT_EQ(0, setenv(kTestVarName, kTestVarValue, /*overwrite=*/1));
  // TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
  // Non-hermetic builds crash when passing a null pointer as the value to
  // setenv.
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  const char* value = nullptr;
  ASSERT_EQ(0, setenv(kVarNullValue, value, /*overwrite=*/1));
#endif
  ASSERT_EQ(0, setenv(kVarEmptyValue, "", /*overwrite=*/1));

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
    if (std::string(*env_ptr) == kTestVarEntry) {
      found_before_unset = true;
      break;
    }
  }
  ASSERT_TRUE(found_before_unset) << "The variable " << kTestVarEntry
                                  << " should be in environ before unset.";

  ASSERT_EQ(0, unsetenv(kTestVarName));

  // Verify it's no longer in environ. How 'environ' itself is modified
  // by unsetenv is implementation-defined.
  bool found_after_unset = false;
  if (environ != nullptr) {
    for (char** env_ptr = environ; *env_ptr != nullptr; ++env_ptr) {
      // Check if the string starts with "kTestVarName="
      // The length for strncmp should be kTestVarName + '='
      if (strncmp(*env_ptr, kTestVarEntry, strlen(kTestVarName) + 1) == 0) {
        found_after_unset = true;
        break;
      }
    }
  }
  ASSERT_FALSE(found_after_unset)
      << "Variable " << kTestVarName
      << " should NOT be in environ after unsetenv.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
