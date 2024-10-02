// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/image_util.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace image_util {
namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

std::string ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    LOG(WARNING) << "Failed reading file";
    result.clear();
  }

  return result;
}

void ToImageSkia(DecodeImageCallback callback, const SkBitmap& bitmap) {
  if (bitmap.empty()) {
    LOG(WARNING) << "Failed to decode image";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  std::move(callback).Run(image);
}

void ToFrames(DecodeAnimationCallback callback,
              std::vector<data_decoder::mojom::AnimationFramePtr> raw_frames) {
  std::vector<AnimationFrame> frames(raw_frames.size());
  base::ranges::transform(
      raw_frames, frames.begin(),
      [](const data_decoder::mojom::AnimationFramePtr& frame_ptr) {
        return AnimationFrame{
            gfx::ImageSkia::CreateFrom1xBitmap(frame_ptr->bitmap),
            frame_ptr->duration};
      });
  std::move(callback).Run(std::move(frames));
}

// EmptyImageSkiaSource --------------------------------------------------------

// An `gfx::ImageSkiaSource` which draws nothing to its `canvas`.
class EmptyImageSkiaSource : public gfx::CanvasImageSource {
 public:
  explicit EmptyImageSkiaSource(const gfx::Size& size)
      : gfx::CanvasImageSource(size) {}

  EmptyImageSkiaSource(const EmptyImageSkiaSource&) = delete;
  EmptyImageSkiaSource& operator=(const EmptyImageSkiaSource&) = delete;
  ~EmptyImageSkiaSource() override = default;

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {}  // Draw nothing.
};

}  // namespace

// Utilities -------------------------------------------------------------------

gfx::ImageSkia CreateEmptyImage(const gfx::Size& size) {
  return gfx::ImageSkia(std::make_unique<EmptyImageSkiaSource>(size), size);
}

void DecodeImageFile(DecodeImageCallback callback,
                     const base::FilePath& file_path,
                     data_decoder::mojom::ImageCodec codec) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToString, file_path),
      base::BindOnce(&DecodeImageData, std::move(callback), codec));
}

void DecodeAnimationFile(DecodeAnimationCallback callback,
                         const base::FilePath& file_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToString, file_path),
      base::BindOnce(&DecodeAnimationData, std::move(callback)));
}

void DecodeImageData(DecodeImageCallback callback,
                     data_decoder::mojom::ImageCodec codec,
                     const std::string& data) {
  if (data.empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  data_decoder::DecodeImageIsolated(
      base::as_bytes(base::make_span(data)), codec,
      /*shrink_to_fit=*/true, kMaxImageSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ToImageSkia, std::move(callback)));
}

void DecodeAnimationData(DecodeAnimationCallback callback,
                         const std::string& data) {
  if (data.empty()) {
    std::move(callback).Run(std::vector<AnimationFrame>());
    return;
  }
  // `shrink_to_fit` is true here so that animations larger than
  // `kMaxImageSizeInBytes` will have their resolution downscaled instead of
  // simply failing to decode.
  data_decoder::DecodeAnimationIsolated(
      base::as_bytes(base::make_span(data)), /*shrink_to_fit=*/true,
      kMaxImageSizeInBytes, base::BindOnce(&ToFrames, std::move(callback)));
}

}  // namespace image_util
}  // namespace ash
