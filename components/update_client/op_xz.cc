// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/op_zucchini.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client_errors.h"
#include "components/zucchini/zucchini.h"

namespace update_client {

namespace {

#if BUILDFLAG(IS_STARBOARD)
void Done(const OperationResult& in_file_result,
          base::OnceCallback<
              void(base::expected<OperationResult, CategorizedError>)> callback,
#else
void Done(base::OnceCallback<
              void(base::expected<base::FilePath, CategorizedError>)> callback,
#endif
          base::RepeatingCallback<void(base::Value::Dict)> event_adder,
          const base::FilePath& out_file,
          bool success) {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventXz);
  event.Set("eventresult",
            static_cast<int>(success ? protocol_request::kEventResultSuccess
                                     : protocol_request::kEventResultError));
  event_adder.Run(std::move(event));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
#if BUILDFLAG(IS_STARBOARD)
          [&]() -> base::expected<OperationResult, CategorizedError> {
            if (success) {
              OperationResult out_result = in_file_result;
              out_result.response = out_file;
              return out_result;
            }
#else
          [&]() -> base::expected<base::FilePath, CategorizedError> {
            if (success) {
              return out_file;
            }
#endif
            return base::unexpected<CategorizedError>(
                {.category = ErrorCategory::kUnpack,
                 .code = static_cast<int>(UnpackerError::kXzFailed)});
          }()));
}

}  // namespace

base::OnceClosure XzOperation(
    std::unique_ptr<Unzipper> unzipper,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
#if BUILDFLAG(IS_STARBOARD)
    const OperationResult& in_file_result,
    base::OnceCallback<void(base::expected<OperationResult, CategorizedError>)>
        callback) {
  const base::FilePath& in_file = in_file_result.response;
#else
    const base::FilePath& in_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
#endif
  base::FilePath dest_file = in_file.DirName().AppendUTF8("decoded_xz");
  Unzipper* unzipper_raw = unzipper.get();
  return unzipper_raw->DecodeXz(
      in_file, dest_file,
      base::BindOnce(
          [](const base::FilePath& in_file, std::unique_ptr<Unzipper> unzipper,
             bool result) {
            base::DeleteFile(in_file);
            return result;
          },
          in_file, std::move(unzipper))
#if BUILDFLAG(IS_STARBOARD)
          .Then(base::BindPostTaskToCurrentDefault(base::BindOnce(
              &Done, in_file_result, std::move(callback), event_adder, dest_file))));
#else
          .Then(base::BindPostTaskToCurrentDefault(base::BindOnce(
              &Done, std::move(callback), event_adder, dest_file))));
#endif
}

}  // namespace update_client
