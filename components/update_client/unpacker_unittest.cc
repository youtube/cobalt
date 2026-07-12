// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unpacker.h"
#if BUILDFLAG(IS_STARBOARD)
#include "components/update_client/pipeline.h"
#include "cobalt/updater/unzipper.h"  // nogncheck
#endif

#include <iterator>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/crx_file/crx_verifier.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_utils.h"
#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/updater/unzipper.h"
#else
#include "components/update_client/unzip/unzip_impl.h"  // nogncheck
#endif
#include "components/update_client/unzipper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class UnpackerTest : public testing::Test {
 private:
  base::test::TaskEnvironment env_;
};

TEST_F(UnpackerTest, UnpackFullCrx) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
#if BUILDFLAG(IS_STARBOARD)
  OperationResult op_result1;
#if defined(IN_MEMORY_UPDATES)
  std::string crx_content1;
  EXPECT_TRUE(base::ReadFileToString(GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &crx_content1));
  op_result1.crx_str = &crx_content1;
  base::FilePath temp_dir1;
  EXPECT_TRUE(base::CreateNewTempDirectory(FILE_PATH_LITERAL("unpacker_test"), &temp_dir1));
  op_result1.installation_dir = temp_dir1;
#else
  base::FilePath temp_dir1;
  EXPECT_TRUE(base::CreateNewTempDirectory(FILE_PATH_LITERAL("unpacker_test"), &temp_dir1));
  op_result1.response = DuplicateTestFile(temp_dir1, "jebgalgnebhfojomionfpkfelancnnkf.crx");
#endif
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      op_result1,
#else
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
#endif
#if BUILDFLAG(USE_EVERGREEN)
      base::MakeRefCounted<cobalt::updater::UnzipperFactory>()->Create(),
#else
      base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))
          ->Create(),
#endif
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kNone);
        EXPECT_EQ(result.extended_error, 0);

        base::FilePath unpack_path = result.unpack_path;
        EXPECT_TRUE(base::DirectoryExists(unpack_path));
        EXPECT_EQ(result.public_key, jebg_public_key);

        std::optional<int64_t> file_size = base::GetFileSize(
            unpack_path.Append(FILE_PATH_LITERAL("component1.dll")));
        ASSERT_TRUE(file_size.has_value());
        EXPECT_EQ(file_size.value(), 1024);
        file_size = base::GetFileSize(
            unpack_path.Append(FILE_PATH_LITERAL("manifest.json")));
        ASSERT_TRUE(file_size.has_value());
        EXPECT_EQ(file_size.value(), 169);

        EXPECT_TRUE(base::DeletePathRecursively(unpack_path));
        loop.Quit();
      }));
  loop.Run();
}

#if !defined(IN_MEMORY_UPDATES)
TEST_F(UnpackerTest, UnpackFileNotFound) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
#if BUILDFLAG(IS_STARBOARD)
  OperationResult op_result2;
  op_result2.response = GetTestFilePath("file_not_found.crx");
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      op_result2, nullptr,
#else
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      GetTestFilePath("file_not_found.crx"), nullptr,
#endif
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kInvalidFile);
        EXPECT_EQ(result.extended_error,
                  static_cast<int>(
                      crx_file::VerifierResult::ERROR_FILE_NOT_READABLE));
        EXPECT_TRUE(result.unpack_path.empty());
        EXPECT_TRUE(result.public_key.empty());
        loop.Quit();
      }));
  loop.Run();
}
#endif  // !defined(IN_MEMORY_UPDATES)

// Tests a mismatch between the public key hash and the id of the component.
TEST_F(UnpackerTest, UnpackFileHashMismatch) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
#if BUILDFLAG(IS_STARBOARD)
  OperationResult op_result3;
#if defined(IN_MEMORY_UPDATES)
  std::string crx_content3;
  EXPECT_TRUE(base::ReadFileToString(GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &crx_content3));
  op_result3.crx_str = &crx_content3;
#else
  op_result3.response = GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx");
#endif
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(abag_hash), std::end(abag_hash)),
      op_result3, nullptr,
#else
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(abag_hash), std::end(abag_hash)),
      GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr,
#endif
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kInvalidFile);
        EXPECT_EQ(result.extended_error,
                  static_cast<int>(
                      crx_file::VerifierResult::ERROR_REQUIRED_PROOF_MISSING));
        EXPECT_TRUE(result.unpack_path.empty());
        EXPECT_TRUE(result.public_key.empty());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(UnpackerTest, UnpackWithVerifiedContents) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
#if BUILDFLAG(IS_STARBOARD)
  OperationResult op_result4;
#if defined(IN_MEMORY_UPDATES)
  std::string crx_content4;
  EXPECT_TRUE(base::ReadFileToString(GetTestFilePath("gndmhdcefbhlchkhipcnnbkcmicncehk_22_314.crx3"), &crx_content4));
  op_result4.crx_str = &crx_content4;
  base::FilePath temp_dir4;
  EXPECT_TRUE(base::CreateNewTempDirectory(FILE_PATH_LITERAL("unpacker_test"), &temp_dir4));
  op_result4.installation_dir = temp_dir4;
#else
  base::FilePath temp_dir4;
  EXPECT_TRUE(base::CreateNewTempDirectory(FILE_PATH_LITERAL("unpacker_test"), &temp_dir4));
  op_result4.response = DuplicateTestFile(temp_dir4, "gndmhdcefbhlchkhipcnnbkcmicncehk_22_314.crx3");
#endif
  Unpacker::Unpack(
      std::vector<uint8_t>(),
      op_result4,
#else
  Unpacker::Unpack(
      std::vector<uint8_t>(),
      GetTestFilePath("gndmhdcefbhlchkhipcnnbkcmicncehk_22_314.crx3"),
#endif
#if BUILDFLAG(USE_EVERGREEN)
      base::MakeRefCounted<cobalt::updater::UnzipperFactory>()->Create(),
#else
      base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))
          ->Create(),
#endif
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kNone);
        base::FilePath unpack_path = result.unpack_path;
        EXPECT_FALSE(unpack_path.empty());
        EXPECT_TRUE(base::DirectoryExists(unpack_path));
        std::optional<int64_t> file_size = base::GetFileSize(
            unpack_path.Append(FILE_PATH_LITERAL("_metadata"))
                .Append(FILE_PATH_LITERAL("verified_contents.json")));
        ASSERT_TRUE(file_size.has_value());
        EXPECT_EQ(file_size.value(), 1538);
        EXPECT_TRUE(base::DeletePathRecursively(unpack_path));
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace update_client
