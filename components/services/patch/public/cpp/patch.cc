// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/public/cpp/patch.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/buildflags.h"
#include "components/update_client/component_patcher_operation.h"  // nogncheck
#include "mojo/public/cpp/bindings/remote.h"

namespace patch {

namespace {

class PatchParams : public base::RefCounted<PatchParams> {
 public:
  PatchParams(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
              PatchCallback callback)
      : file_patcher_(std::move(file_patcher)),
        callback_(std::move(callback)) {}

  PatchParams(const PatchParams&) = delete;
  PatchParams& operator=(const PatchParams&) = delete;

  mojo::Remote<mojom::FilePatcher>& file_patcher() { return file_patcher_; }

  PatchCallback TakeCallback() { return std::move(callback_); }

 private:
  friend class base::RefCounted<PatchParams>;

  ~PatchParams() = default;

  // The mojo::Remote<FilePatcher> is stored so it does not get deleted before
  // the callback runs.
  mojo::Remote<mojom::FilePatcher> file_patcher_;

  PatchCallback callback_;
};

void PatchDone(scoped_refptr<PatchParams> params, int result) {
  params->file_patcher().reset();
  PatchCallback cb = params->TakeCallback();
  if (!cb.is_null())
    std::move(cb).Run(result);
}

}  // namespace

// TODO(crbug.com/1349158): Remove this function once PatchFilePuffPatch is
// implemented as this becomes obsolete.
void Patch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
           const std::string& operation,
           const base::FilePath& input_path,
           const base::FilePath& patch_path,
           const base::FilePath& output_path,
           PatchCallback callback) {
  DCHECK(!callback.is_null());

  base::File input_file(input_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File patch_file(patch_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File output_file(output_path, base::File::FLAG_CREATE |
                                          base::File::FLAG_WRITE |
                                          base::File::FLAG_WIN_EXCLUSIVE_WRITE);

  if (!input_file.IsValid() || !patch_file.IsValid() ||
      !output_file.IsValid()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*result=*/-1));
    return;
  }

  // In order to share |callback| between the connection error handler and the
  // FilePatcher calls, we have to use a context object.
  scoped_refptr<PatchParams> patch_params = base::MakeRefCounted<PatchParams>(
      std::move(file_patcher), std::move(callback));

  patch_params->file_patcher().set_disconnect_handler(
      base::BindOnce(&PatchDone, patch_params, /*result=*/-1));

  if (operation == update_client::kBsdiff) {
    patch_params->file_patcher()->PatchFileBsdiff(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        base::BindOnce(&PatchDone, patch_params));
  } else if (operation == update_client::kCourgette) {
    patch_params->file_patcher()->PatchFileCourgette(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        base::BindOnce(&PatchDone, patch_params));
  } else {
    NOTREACHED();
  }
}

void PuffPatch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
               base::File input_file,
               base::File patch_file,
               base::File output_file,
               PatchCallback callback) {
#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
  // TODO(crbug.com/1349060) once Puffin patches are fully implemented,
  // we should remove this #if.

  // Use a context object to share callback.
  scoped_refptr<PatchParams> patch_params = base::MakeRefCounted<PatchParams>(
      std::move(file_patcher), std::move(callback));

  patch_params->file_patcher().set_disconnect_handler(
      base::BindOnce(&PatchDone, patch_params, /*result=*/-1));

  patch_params->file_patcher()->PatchFilePuffPatch(
      std::move(input_file), std::move(patch_file), std::move(output_file),
      base::BindOnce(&PatchDone, patch_params));
#endif
}

}  // namespace patch
