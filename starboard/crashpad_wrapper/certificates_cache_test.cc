// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/crashpad_wrapper/certificates_cache.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crashpad {
namespace {

class SyncCACertificatesCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(package_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(cache_dir_.CreateUniqueTempDir());
    certs_path_ =
        package_dir_.GetPath().Append("content").Append("ssl").Append("certs");
    copy_path_ = cache_dir_.GetPath().Append("certs");
    ASSERT_TRUE(base::CreateDirectory(certs_path_));
    WriteManifest("1.0");
    WriteCertificate("a.0", "certificate a");
    WriteCertificate("b.0", "certificate b");
  }

  void WriteManifest(const std::string& version) {
    ASSERT_TRUE(base::WriteFile(package_dir_.GetPath().Append("manifest.json"),
                                "{\"version\": \"" + version + "\"}"));
  }

  void WriteCertificate(const std::string& name, const std::string& contents) {
    ASSERT_TRUE(base::WriteFile(certs_path_.Append(name), contents));
  }

  std::optional<base::FilePath> Sync() {
    return SyncCACertificatesCache(certs_path_.value(),
                                   cache_dir_.GetPath().value());
  }

  // Changes the copy of `name`, so that a test can tell whether the next
  // Sync() copies it again.
  void MarkCopy(const std::string& name) {
    ASSERT_TRUE(base::WriteFile(copy_path_.Append(name), "marked"));
  }

  std::string ReadCopy(const std::string& name) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(copy_path_.Append(name), &contents));
    return contents;
  }

  base::ScopedTempDir package_dir_;
  base::ScopedTempDir cache_dir_;
  base::FilePath certs_path_;
  base::FilePath copy_path_;
};

TEST_F(SyncCACertificatesCacheTest, CopiesCertificates) {
  EXPECT_EQ(Sync(), copy_path_);
  EXPECT_EQ(ReadCopy("a.0"), "certificate a");
  EXPECT_EQ(ReadCopy("b.0"), "certificate b");
}

TEST_F(SyncCACertificatesCacheTest, ReusesCopyWhenUnchanged) {
  ASSERT_EQ(Sync(), copy_path_);
  MarkCopy("a.0");

  EXPECT_EQ(Sync(), copy_path_);
  EXPECT_EQ(ReadCopy("a.0"), "marked");
}

TEST_F(SyncCACertificatesCacheTest, CopiesAgainWhenManifestChanges) {
  ASSERT_EQ(Sync(), copy_path_);
  MarkCopy("a.0");
  WriteManifest("2.0");

  EXPECT_EQ(Sync(), copy_path_);
  EXPECT_EQ(ReadCopy("a.0"), "certificate a");
}

TEST_F(SyncCACertificatesCacheTest, CopiesAgainWhenCertificateNamesChange) {
  ASSERT_EQ(Sync(), copy_path_);
  MarkCopy("a.0");
  ASSERT_TRUE(base::DeleteFile(certs_path_.Append("b.0")));
  WriteCertificate("c.0", "certificate c");

  EXPECT_EQ(Sync(), copy_path_);
  EXPECT_EQ(ReadCopy("a.0"), "certificate a");
  EXPECT_FALSE(base::PathExists(copy_path_.Append("b.0")));
  EXPECT_EQ(ReadCopy("c.0"), "certificate c");
}

TEST_F(SyncCACertificatesCacheTest, CopiesAgainWhenCopiedFileIsMissing) {
  ASSERT_EQ(Sync(), copy_path_);
  MarkCopy("a.0");
  ASSERT_TRUE(base::DeleteFile(copy_path_.Append("b.0")));

  EXPECT_EQ(Sync(), copy_path_);
  EXPECT_EQ(ReadCopy("a.0"), "certificate a");
  EXPECT_EQ(ReadCopy("b.0"), "certificate b");
}

TEST_F(SyncCACertificatesCacheTest, CopiesEveryTimeWithoutManifest) {
  ASSERT_TRUE(base::DeleteFile(package_dir_.GetPath().Append("manifest.json")));
  ASSERT_EQ(Sync(), copy_path_);
  MarkCopy("a.0");

  EXPECT_EQ(Sync(), copy_path_);
  EXPECT_EQ(ReadCopy("a.0"), "certificate a");
}

TEST_F(SyncCACertificatesCacheTest, FailsWithoutCertificates) {
  ASSERT_TRUE(base::DeletePathRecursively(certs_path_));
  EXPECT_EQ(Sync(), std::nullopt);
}

TEST_F(SyncCACertificatesCacheTest, FailsWithEmptyPaths) {
  EXPECT_EQ(SyncCACertificatesCache("", cache_dir_.GetPath().value()),
            std::nullopt);
  EXPECT_EQ(SyncCACertificatesCache(certs_path_.value(), ""), std::nullopt);
}

}  // namespace
}  // namespace crashpad
