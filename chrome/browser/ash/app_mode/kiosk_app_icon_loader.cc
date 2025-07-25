// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_icon_loader.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace ash {

class IconImageRequest : public ImageDecoder::ImageRequest {
 public:
  IconImageRequest(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   KioskAppIconLoader::ResultCallback result_callback)
      : ImageRequest(task_runner),
        result_callback_(std::move(result_callback)) {}

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
    image.MakeThreadSafe();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback_), image));
    delete this;
  }

  void OnDecodeImageFailed() override {
    LOG(ERROR) << "Failed to decode icon image.";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback_),
                                  absl::optional<gfx::ImageSkia>()));
    delete this;
  }

 private:
  ~IconImageRequest() override = default;
  KioskAppIconLoader::ResultCallback result_callback_;
};

void LoadOnBlockingPool(
    const base::FilePath& icon_path,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    KioskAppIconLoader::ResultCallback result_callback) {
  DCHECK(callback_task_runner->RunsTasksInCurrentSequence());

  std::string data;
  if (!base::ReadFileToString(base::FilePath(icon_path), &data)) {
    LOG(ERROR) << "Failed to read icon file.";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  absl::optional<gfx::ImageSkia>()));
    return;
  }

  // IconImageRequest will delete itself on completion of ImageDecoder callback.
  IconImageRequest* image_request =
      new IconImageRequest(callback_task_runner, std::move(result_callback));
  ImageDecoder::Start(image_request, std::move(data));
}

KioskAppIconLoader::KioskAppIconLoader(ResultCallback callback)
    : callback_(std::move(callback)) {}

KioskAppIconLoader::~KioskAppIconLoader() = default;

void KioskAppIconLoader::Start(const base::FilePath& icon_path) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LoadOnBlockingPool, icon_path, task_runner,
          base::BindOnce(&KioskAppIconLoader::OnImageDecodingFinished,
                         weak_factory_.GetWeakPtr())));
}

void KioskAppIconLoader::OnImageDecodingFinished(
    absl::optional<gfx::ImageSkia> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback_).Run(result);
}

}  // namespace ash
