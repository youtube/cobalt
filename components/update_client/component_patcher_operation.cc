// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component_patcher_operation.h"

#include <stdint.h>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/patcher.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace update_client {

namespace {

const char kOutput[] = "output";
const char kSha256[] = "sha256";

// The integer offset disambiguates between overlapping error ranges.
const int kCourgetteErrorOffset = 300;
const int kBsdiffErrorOffset = 600;

}  // namespace

const char kOp[] = "op";
const char kBsdiff[] = "bsdiff";
const char kCourgette[] = "courgette";
const char kInput[] = "input";
const char kPatch[] = "patch";

DeltaUpdateOp* CreateDeltaUpdateOp(const std::string& operation,
                                   scoped_refptr<Patcher> patcher) {
  if (operation == "copy") {
    return new DeltaUpdateOpCopy();
  } else if (operation == "create") {
    return new DeltaUpdateOpCreate();
  } else if (operation == "bsdiff" || operation == "courgette") {
    return new DeltaUpdateOpPatch(operation, patcher);
  }
  return nullptr;
}

DeltaUpdateOp::DeltaUpdateOp() = default;

DeltaUpdateOp::~DeltaUpdateOp() = default;

void DeltaUpdateOp::Run(const base::Value::Dict& command_args,
                        const base::FilePath& input_dir,
                        const base::FilePath& unpack_dir,
                        scoped_refptr<CrxInstaller> installer,
                        ComponentPatcher::Callback callback) {
  callback_ = std::move(callback);
  const std::string* output_rel_path = command_args.FindString(kOutput);
  const std::string* sha256_value = command_args.FindString(kSha256);
  if (!output_rel_path || !sha256_value) {
    DoneRunning(UnpackerError::kDeltaBadCommands, 0);
    return;
  }
  output_sha256_ = *sha256_value;

  output_abs_path_ =
      unpack_dir.Append(base::FilePath::FromUTF8Unsafe(*output_rel_path));
  UnpackerError parse_result =
      DoParseArguments(command_args, input_dir, installer);
  if (parse_result != UnpackerError::kNone) {
    DoneRunning(parse_result, 0);
    return;
  }

  const base::FilePath parent = output_abs_path_.DirName();
  if (!base::DirectoryExists(parent)) {
    if (!base::CreateDirectory(parent)) {
      DoneRunning(UnpackerError::kIoError, 0);
      return;
    }
  }

  DoRun(base::BindOnce(&DeltaUpdateOp::DoneRunning,
                       scoped_refptr<DeltaUpdateOp>(this)));
}

void DeltaUpdateOp::DoneRunning(UnpackerError error, int extended_error) {
  if (error == UnpackerError::kNone)
    error = CheckHash();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), error, extended_error));
}

// Uses the hash as a checksum to confirm that the file now residing in the
// output directory probably has the contents it should.
UnpackerError DeltaUpdateOp::CheckHash() {
  return VerifyFileHash256(output_abs_path_, output_sha256_)
             ? UnpackerError::kNone
             : UnpackerError::kDeltaVerificationFailure;
}

DeltaUpdateOpCopy::DeltaUpdateOpCopy() = default;

DeltaUpdateOpCopy::~DeltaUpdateOpCopy() = default;

UnpackerError DeltaUpdateOpCopy::DoParseArguments(
    const base::Value::Dict& command_args,
    const base::FilePath& input_dir,
    scoped_refptr<CrxInstaller> installer) {
  const std::string* input_rel_path = command_args.FindString(kInput);
  if (!input_rel_path)
    return UnpackerError::kDeltaBadCommands;

  if (!installer->GetInstalledFile(*input_rel_path, &input_abs_path_))
    return UnpackerError::kDeltaMissingExistingFile;

  return UnpackerError::kNone;
}

void DeltaUpdateOpCopy::DoRun(ComponentPatcher::Callback callback) {
  if (!base::CopyFile(input_abs_path_, output_abs_path_))
    std::move(callback).Run(UnpackerError::kDeltaOperationFailure, 0);
  else
    std::move(callback).Run(UnpackerError::kNone, 0);
}

DeltaUpdateOpCreate::DeltaUpdateOpCreate() = default;

DeltaUpdateOpCreate::~DeltaUpdateOpCreate() = default;

UnpackerError DeltaUpdateOpCreate::DoParseArguments(
    const base::Value::Dict& command_args,
    const base::FilePath& input_dir,
    scoped_refptr<CrxInstaller> installer) {
  const std::string* patch_rel_path = command_args.FindString(kPatch);
  if (!patch_rel_path)
    return UnpackerError::kDeltaBadCommands;

  patch_abs_path_ =
      input_dir.Append(base::FilePath::FromUTF8Unsafe(*patch_rel_path));

  return UnpackerError::kNone;
}

void DeltaUpdateOpCreate::DoRun(ComponentPatcher::Callback callback) {
  if (!base::Move(patch_abs_path_, output_abs_path_))
    std::move(callback).Run(UnpackerError::kDeltaOperationFailure, 0);
  else
    std::move(callback).Run(UnpackerError::kNone, 0);
}

DeltaUpdateOpPatch::DeltaUpdateOpPatch(const std::string& operation,
                                       scoped_refptr<Patcher> patcher)
    : operation_(operation), patcher_(patcher) {
  CHECK(operation == kBsdiff || operation == kCourgette);
}

DeltaUpdateOpPatch::~DeltaUpdateOpPatch() = default;

UnpackerError DeltaUpdateOpPatch::DoParseArguments(
    const base::Value::Dict& command_args,
    const base::FilePath& input_dir,
    scoped_refptr<CrxInstaller> installer) {
  const std::string* patch_rel_path = command_args.FindString(kPatch);
  const std::string* input_rel_path = command_args.FindString(kInput);
  if (!patch_rel_path || !input_rel_path)
    return UnpackerError::kDeltaBadCommands;

  if (!installer->GetInstalledFile(*input_rel_path, &input_abs_path_))
    return UnpackerError::kDeltaMissingExistingFile;

  patch_abs_path_ =
      input_dir.Append(base::FilePath::FromUTF8Unsafe(*patch_rel_path));

  return UnpackerError::kNone;
}

void DeltaUpdateOpPatch::DoRun(ComponentPatcher::Callback callback) {
  if (operation_ == kBsdiff) {
    patcher_->PatchBsdiff(input_abs_path_, patch_abs_path_, output_abs_path_,
                          base::BindOnce(&DeltaUpdateOpPatch::DonePatching,
                                         this, std::move(callback)));
  } else {
    patcher_->PatchCourgette(input_abs_path_, patch_abs_path_, output_abs_path_,
                             base::BindOnce(&DeltaUpdateOpPatch::DonePatching,
                                            this, std::move(callback)));
  }
}

void DeltaUpdateOpPatch::DonePatching(ComponentPatcher::Callback callback,
                                      int result) {
  if (operation_ == kBsdiff) {
    if (result == bsdiff::OK) {
      std::move(callback).Run(UnpackerError::kNone, 0);
    } else {
      std::move(callback).Run(UnpackerError::kDeltaOperationFailure,
                              result + kBsdiffErrorOffset);
    }
  } else if (operation_ == kCourgette) {
    if (result == courgette::C_OK) {
      std::move(callback).Run(UnpackerError::kNone, 0);
    } else {
      std::move(callback).Run(UnpackerError::kDeltaOperationFailure,
                              result + kCourgetteErrorOffset);
    }
  } else {
    NOTREACHED();
  }
}

}  // namespace update_client
