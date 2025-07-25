// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/test_utils.h"

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace psm_rlwe = private_membership::rlwe;

using psm_rlwe_test =
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase;

using pc_preserved_file_test =
    private_computing::PrivateComputingClientRegressionTestData;

namespace ash::report::utils {

namespace {

bool ProtosAreEqual(const google::protobuf::MessageLite& actual,
                    const google::protobuf::MessageLite& expected) {
  return (actual.GetTypeName() == expected.GetTypeName()) &&
         (actual.SerializeAsString() == expected.SerializeAsString());
}

}  // namespace

class TestUtilsTest : public testing::Test {
 public:
  static psm_rlwe::PrivateMembershipRlweClientRegressionTestData*
  GetPsmTestData() {
    static base::NoDestructor<
        psm_rlwe::PrivateMembershipRlweClientRegressionTestData>
        data;
    return data.get();
  }

  static private_computing::PrivateComputingClientRegressionTestData*
  GetPreservedFileTestData() {
    static base::NoDestructor<
        private_computing::PrivateComputingClientRegressionTestData>
        preserved_file_test_data;
    return preserved_file_test_data.get();
  }

  static void CreatePsmTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
    const base::FilePath kPsmTestDataPath =
        src_root_dir.AppendASCII("third_party")
            .AppendASCII("private_membership")
            .AppendASCII("src")
            .AppendASCII("internal")
            .AppendASCII("testing")
            .AppendASCII("regression_test_data")
            .AppendASCII("test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPsmTestDataPath));
    ASSERT_TRUE(utils::ParseProtoFromFile(kPsmTestDataPath, GetPsmTestData()));

    ASSERT_EQ(GetPsmTestData()->test_cases_size(), utils::kPsmTestCaseSize);
  }

  static void CreatePreservedFileTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
    const base::FilePath kPrivateComputingTestDataPath =
        src_root_dir.AppendASCII("chromeos")
            .AppendASCII("ash")
            .AppendASCII("components")
            .AppendASCII("report")
            .AppendASCII("device_metrics")
            .AppendASCII("testing")
            .AppendASCII("preserved_file_test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPrivateComputingTestDataPath));
    ASSERT_TRUE(utils::ParseProtoFromFile(kPrivateComputingTestDataPath,
                                          GetPreservedFileTestData()));

    // Note that the test cases can change since it's read from the binary pb.
    ASSERT_EQ(GetPreservedFileTestData()->test_cases_size(),
              utils::kPreservedFileTestCaseSize);
  }

  static void SetUpTestSuite() {
    // Initialize PSM test data used to fake check membership flow.
    CreatePsmTestData();

    // Initialize preserved file test data.
    CreatePreservedFileTestData();
  }

  void SetUp() override {
    // Initialize source root directory.
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir_));
  }

 protected:
  const base::FilePath& GetSrcRootDirectory() { return src_root_dir_; }

 private:
  base::FilePath src_root_dir_;
};

TEST_F(TestUtilsTest, ParseProtoFromFileValidFileReturnsTrue) {
  // Read valid preserved file test data content.
  {
    const base::FilePath file_path =
        GetSrcRootDirectory()
            .AppendASCII("chromeos")
            .AppendASCII("ash")
            .AppendASCII("components")
            .AppendASCII("report")
            .AppendASCII("device_metrics")
            .AppendASCII("testing")
            .AppendASCII("preserved_file_test_data.binarypb");

    private_computing::PrivateComputingClientRegressionTestData
        private_computing_test_data;
    EXPECT_TRUE(ParseProtoFromFile(file_path, &private_computing_test_data));
  }

  // Read valid psm test data content.
  {
    base::FilePath file_path = GetSrcRootDirectory()
                                   .AppendASCII("third_party")
                                   .AppendASCII("private_membership")
                                   .AppendASCII("src")
                                   .AppendASCII("internal")
                                   .AppendASCII("testing")
                                   .AppendASCII("regression_test_data")
                                   .AppendASCII("test_data.binarypb");

    psm_rlwe::PrivateMembershipRlweClientRegressionTestData psm_test_data;
    EXPECT_TRUE(ParseProtoFromFile(file_path, &psm_test_data));
  }
}

TEST_F(TestUtilsTest, ParseProtoFromFileInvalidFileReturnsFalse) {
  // Read invalid preserved file test data content.
  {
    const base::FilePath file_path =
        GetSrcRootDirectory().AppendASCII("fake_preserved_file_path");

    private_computing::PrivateComputingClientRegressionTestData
        private_computing_test_data;
    EXPECT_FALSE(ParseProtoFromFile(file_path, &private_computing_test_data));
  }

  // Read invalid psm test data content.
  {
    base::FilePath file_path =
        GetSrcRootDirectory().AppendASCII("fake_psm_path");

    psm_rlwe::PrivateMembershipRlweClientRegressionTestData psm_test_data;
    EXPECT_FALSE(ParseProtoFromFile(file_path, &psm_test_data));
  }
}

TEST_F(TestUtilsTest, GetPsmTestCase) {
  {
    int test_idx = 0;  // Provide a valid test index.
    auto test_case = GetPsmTestCase(GetPsmTestData(), test_idx);
    EXPECT_TRUE(test_case.IsInitialized());
  }

  {
    int test_idx_invalid = 11;  // Provide an invalid test index.
    auto test_case_invalid = GetPsmTestCase(GetPsmTestData(), test_idx_invalid);
    EXPECT_TRUE(
        ProtosAreEqual(test_case_invalid,
                       psm_rlwe::PrivateMembershipRlweClientRegressionTestData::
                           TestCase::default_instance()));
  }

  {
    int test_idx_invalid = -1;  // Provide an invalid test index.
    auto test_case_invalid = GetPsmTestCase(GetPsmTestData(), test_idx_invalid);
    EXPECT_TRUE(
        ProtosAreEqual(test_case_invalid,
                       psm_rlwe::PrivateMembershipRlweClientRegressionTestData::
                           TestCase::default_instance()));
  }
}

TEST_F(TestUtilsTest, GetPreservedFileTestCase) {
  {
    auto test_case =
        GetPreservedFileTestCase(GetPreservedFileTestData(),
                                 pc_preserved_file_test::GET_FAIL_SAVE_SUCCESS);
    EXPECT_TRUE(test_case.IsInitialized());
  }
}

}  // namespace ash::report::utils
