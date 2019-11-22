// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component_unpacker.h"

#include <stdint.h>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component_patcher.h"
#include "components/update_client/patcher.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

#if defined(OS_STARBOARD)
#include "cobalt/extension/installation_manager.h"
#endif

#if defined(OS_STARBOARD)
namespace {
void CleanupDirectory(base::FilePath& dir) {
  std::stack<std::string> directories;
  base::FileEnumerator file_enumerator(
      dir, true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (auto path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::FileEnumerator::FileInfo info(file_enumerator.GetInfo());

    if (info.IsDirectory()) {
      directories.push(path.value());
    } else {
      SbFileDelete(path.value().c_str());
    }
  }
  while (!directories.empty()) {
    SbFileDelete(directories.top().c_str());
    directories.pop();
  }
}
}  // namespace
#endif

namespace update_client {

ComponentUnpacker::Result::Result() {}

ComponentUnpacker::ComponentUnpacker(const std::vector<uint8_t>& pk_hash,
                                     const base::FilePath& path,
                                     scoped_refptr<CrxInstaller> installer,
                                     std::unique_ptr<Unzipper> unzipper,
                                     scoped_refptr<Patcher> patcher,
                                     crx_file::VerifierFormat crx_format)
    : pk_hash_(pk_hash),
      path_(path),
      is_delta_(false),
      installer_(installer),
      unzipper_(std::move(unzipper)),
      patcher_tool_(patcher),
      crx_format_(crx_format),
      error_(UnpackerError::kNone),
      extended_error_(0) {}

ComponentUnpacker::~ComponentUnpacker() {}

void ComponentUnpacker::Unpack(Callback callback) {
  callback_ = std::move(callback);
  if (!Verify() || !BeginUnzipping())
    EndUnpacking();
}

bool ComponentUnpacker::Verify() {
  VLOG(1) << "Verifying component: " << path_.value();

  if (path_.empty()) {
    error_ = UnpackerError::kInvalidParams;
    return false;
  }
  std::vector<std::vector<uint8_t>> required_keys;
  if (!pk_hash_.empty())
    required_keys.push_back(pk_hash_);
  const crx_file::VerifierResult result =
      crx_file::Verify(path_, crx_format_, required_keys,
                       std::vector<uint8_t>(), &public_key_, nullptr);
  if (result != crx_file::VerifierResult::OK_FULL &&
      result != crx_file::VerifierResult::OK_DELTA) {
    error_ = UnpackerError::kInvalidFile;
    extended_error_ = static_cast<int>(result);
    SB_LOG(INFO) << "Verification failed. Verifier error = " << extended_error_;
    return false;
  }
  is_delta_ = result == crx_file::VerifierResult::OK_DELTA;
  VLOG(1) << "Verification successful: " << path_.value();
  return true;
}

bool ComponentUnpacker::BeginUnzipping() {
  // Mind the reference to non-const type, passed as an argument below.
  base::FilePath& destination = is_delta_ ? unpack_diff_path_ : unpack_path_;

#if !defined(OS_STARBOARD)
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                    &destination)) {
    VLOG(1) << "Unable to create temporary directory for unpacking.";
    error_ = UnpackerError::kUnzipPathError;
    return false;
  }
#else
  const CobaltExtensionInstallationManagerApi* installation_api =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_api) {
    // TODO: add correct error code.
    error_ = UnpackerError::kUnzipPathError;
    return false;
  }
  // Get new installation index.
  int installation_index = installation_api->SelectNewInstallationIndex();
  SB_DLOG(INFO) << "installation_index=" << installation_index;
  if (installation_index == IM_EXT_ERROR) {
    error_ = UnpackerError::kUnzipPathError;
    return false;
  }

  // Get the path to new installation.
  char installation_path[SB_FILE_MAX_PATH];
  if (installation_api->GetInstallationPath(installation_index,
                                            installation_path,
                                            SB_FILE_MAX_PATH) == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get installation path";
    error_ = UnpackerError::kUnzipPathError;
    return false;
  }

  SB_DLOG(INFO) << "installation_path=" << installation_path;
  destination = base::FilePath(installation_path);

  // Cleanup the unpack path.
  CleanupDirectory(destination);

#endif
  VLOG(1) << "Unpacking in: " << destination.value();
  unzipper_->Unzip(path_, destination,
                   base::BindOnce(&ComponentUnpacker::EndUnzipping, this));

#if defined(OS_STARBOARD)
  int ret =
      installation_api->RequestRollForwardToInstallation(installation_index);
  if (ret == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to request roll forward";
  }
#endif
  return true;
}

void ComponentUnpacker::EndUnzipping(bool result) {
  if (!result) {
    VLOG(1) << "Unzipping failed.";
    error_ = UnpackerError::kUnzipFailed;
    EndUnpacking();
    return;
  }
  VLOG(1) << "Unpacked successfully";
  BeginPatching();
}

bool ComponentUnpacker::BeginPatching() {
  if (is_delta_) {  // Package is a diff package.
    // Use a different temp directory for the patch output files.
    if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                      &unpack_path_)) {
      error_ = UnpackerError::kUnzipPathError;
      return false;
    }
    patcher_ = base::MakeRefCounted<ComponentPatcher>(
        unpack_diff_path_, unpack_path_, installer_, patcher_tool_);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ComponentPatcher::Start, patcher_,
                       base::BindOnce(&ComponentUnpacker::EndPatching,
                                      scoped_refptr<ComponentUnpacker>(this))));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ComponentUnpacker::EndPatching,
                                  scoped_refptr<ComponentUnpacker>(this),
                                  UnpackerError::kNone, 0));
  }
  return true;
}

void ComponentUnpacker::EndPatching(UnpackerError error, int extended_error) {
  error_ = error;
  extended_error_ = extended_error;
  patcher_ = nullptr;

  EndUnpacking();
}

void ComponentUnpacker::EndUnpacking() {
#if !defined(OS_STARBOARD)
  if (!unpack_diff_path_.empty())
    base::DeleteFile(unpack_diff_path_, true);
  if (error_ != UnpackerError::kNone && !unpack_path_.empty())
    base::DeleteFile(unpack_path_, true);
#endif

  Result result;
  result.error = error_;
  result.extended_error = extended_error_;
  if (error_ == UnpackerError::kNone) {
    result.unpack_path = unpack_path_;
    result.public_key = public_key_;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), result));
}

}  // namespace update_client
