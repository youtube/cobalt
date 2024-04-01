// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/offloading_video_encoder.h"

#include "base/bind_post_task.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/video_frame.h"

namespace media {

OffloadingVideoEncoder::OffloadingVideoEncoder(
    std::unique_ptr<VideoEncoder> wrapped_encoder,
    const scoped_refptr<base::SequencedTaskRunner> work_runner,
    const scoped_refptr<base::SequencedTaskRunner> callback_runner)
    : wrapped_encoder_(std::move(wrapped_encoder)),
      work_runner_(std::move(work_runner)),
      callback_runner_(std::move(callback_runner)) {
  DCHECK(wrapped_encoder_);
  DCHECK(work_runner_);
  DCHECK(callback_runner_);
  DCHECK_NE(callback_runner_, work_runner_);
}

OffloadingVideoEncoder::OffloadingVideoEncoder(
    std::unique_ptr<VideoEncoder> wrapped_encoder)
    : OffloadingVideoEncoder(std::move(wrapped_encoder),
                             base::ThreadPool::CreateSequencedTaskRunner(
                                 {base::TaskPriority::USER_BLOCKING,
                                  base::WithBaseSyncPrimitives()}),
                             base::SequencedTaskRunnerHandle::Get()) {}

void OffloadingVideoEncoder::Initialize(VideoCodecProfile profile,
                                        const Options& options,
                                        OutputCB output_cb,
                                        StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoder::Initialize,
                     base::Unretained(wrapped_encoder_.get()), profile, options,
                     WrapCallback(std::move(output_cb)),
                     WrapCallback(std::move(done_cb))));
}

void OffloadingVideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                                    bool key_frame,
                                    StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoder::Encode,
                     base::Unretained(wrapped_encoder_.get()), std::move(frame),
                     key_frame, WrapCallback(std::move(done_cb))));
}

void OffloadingVideoEncoder::ChangeOptions(const Options& options,
                                           OutputCB output_cb,
                                           StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoder::ChangeOptions,
                                base::Unretained(wrapped_encoder_.get()),
                                options, WrapCallback(std::move(output_cb)),
                                WrapCallback(std::move(done_cb))));
}

void OffloadingVideoEncoder::Flush(StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoder::Flush,
                                base::Unretained(wrapped_encoder_.get()),
                                WrapCallback(std::move(done_cb))));
}

OffloadingVideoEncoder::~OffloadingVideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->DeleteSoon(FROM_HERE, std::move(wrapped_encoder_));
}

template <class T>
T OffloadingVideoEncoder::WrapCallback(T cb) {
  DCHECK(callback_runner_);
  return base::BindPostTask(callback_runner_, std::move(cb));
}

}  // namespace media
