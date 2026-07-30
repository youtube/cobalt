// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/storage_util.h"

#include <array>
#include <string>
#include <utility>
#include <variant>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "components/base32/base32.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace web_app {

namespace {

constexpr unsigned kRandomDirNameOctetsLength = 10;

// Returns a base32 representation of 80 random bits. This leads
// to the 16 characters long directory name. 80 bits should be long
// enough not to care about collisions.
std::string GenerateRandomDirName() {
  std::array<uint8_t, kRandomDirNameOctetsLength> random_array;
  crypto::RandBytes(random_array);
  return base::ToLowerASCII(base32::Base32Encode(
      random_array, base32::Base32EncodePolicy::OMIT_PADDING));
}

enum class Operation { kCopy, kMove };

base::expected<IsolatedWebAppStorageLocation, std::string>
CopyOrMoveSwbnToIwaDir(const base::FilePath& swbn_path,
                       const base::FilePath& profile_dir,
                       bool dev_mode,
                       Operation operation) {
  const base::FilePath iwa_dir_path = profile_dir.Append(kIwaDirName);
  if (!base::DirectoryExists(iwa_dir_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(iwa_dir_path, &error)) {
      return base::unexpected("Failed to create a root IWA directory: " +
                              base::File::ErrorToString(error));
    }
  }

  std::string dir_name_ascii = GenerateRandomDirName();
  const base::FilePath destination_dir =
      iwa_dir_path.AppendASCII(dir_name_ascii);
  if (base::DirectoryExists(destination_dir)) {
    return base::unexpected("The unique destination directory exists: " +
                            destination_dir.AsUTF8Unsafe());
  }

  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(destination_dir, &error)) {
    return base::unexpected(
        "Failed to create a directory " + destination_dir.AsUTF8Unsafe() +
        " for the IWA: " + base::File::ErrorToString(error));
  }

  const base::FilePath destination_swbn_path =
      destination_dir.Append(kMainSwbnFileName);
  switch (operation) {
    case Operation::kCopy:
      if (!base::CopyFile(swbn_path, destination_swbn_path)) {
        base::DeletePathRecursively(destination_dir);
        return base::unexpected(
            "Failed to copy the " + swbn_path.AsUTF8Unsafe() + " file to the " +
            destination_swbn_path.AsUTF8Unsafe() + " IWA directory");
      }
      break;
    case Operation::kMove:
      if (!base::Move(swbn_path, destination_swbn_path)) {
        base::DeletePathRecursively(destination_dir);
        return base::unexpected(
            "Failed to move the " + swbn_path.AsUTF8Unsafe() + " file to the " +
            destination_swbn_path.AsUTF8Unsafe() + " IWA directory");
      }
      break;
  }
  return IwaStorageOwnedBundle{dir_name_ascii, dev_mode};
}

void RemoveParentDirectory(const base::FilePath& path) {
  base::FilePath dir_path = path.DirName();
  if (!base::DeletePathRecursively(dir_path)) {
    LOG(ERROR) << "Could not delete " << dir_path;
  }
}

}  // namespace

void CleanupLocationIfOwned(const base::FilePath& profile_dir,
                            const IsolatedWebAppStorageLocation& location,
                            base::OnceClosure closure) {
  std::visit(
      absl::Overload{
          [&](const IwaStorageOwnedBundle& location) {
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                 base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                base::BindOnce(RemoveParentDirectory,
                               location.GetPath(profile_dir)),
                std::move(closure));
          },
          [&](const IwaStorageUnownedBundle& location) {
            std::move(closure).Run();
          },
          [&](const IwaStorageProxy& location) { std::move(closure).Run(); }},
      location.variant());
}

void UpdateBundlePathAndCreateStorageLocation(
    const base::FilePath& profile_dir,
    const IwaSourceWithModeAndFileOp& source,
    base::OnceCallback<void(
        base::expected<IsolatedWebAppStorageLocation, std::string>)> callback) {
  auto copy_or_move = [&callback, &profile_dir](
                          const base::FilePath& bundle_path, bool dev_mode,
                          Operation operation) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(CopyOrMoveSwbnToIwaDir, bundle_path, profile_dir,
                       dev_mode, operation),
        std::move(callback));
  };

  std::visit(absl::Overload{
                 [&](const IwaSourceBundleWithModeAndFileOp& bundle) {
                   switch (bundle.mode_and_file_op()) {
                     case IwaSourceBundleModeAndFileOp::kDevModeCopy:
                       copy_or_move(bundle.path(), /*dev_mode=*/true,
                                    Operation::kCopy);
                       break;
                     case IwaSourceBundleModeAndFileOp::kDevModeMove:
                       copy_or_move(bundle.path(), /*dev_mode=*/true,
                                    Operation::kMove);
                       break;
                     case IwaSourceBundleModeAndFileOp::kProdModeCopy:
                       copy_or_move(bundle.path(), /*dev_mode=*/false,
                                    Operation::kCopy);
                       break;
                     case IwaSourceBundleModeAndFileOp::kProdModeMove:
                       copy_or_move(bundle.path(), /*dev_mode=*/false,
                                    Operation::kMove);
                       break;
                   }
                 },
                 [&](const IwaSourceProxy& proxy) {
                   std::move(callback).Run(IwaStorageProxy(proxy.proxy_url()));
                 },
             },
             source.variant());
}

}  // namespace web_app
