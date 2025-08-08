// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_async_wrapper_ios.h"

#include "base/task/thread_pool.h"

void GetGeneralPasteboard(bool asynchronous, PasteboardCallback callback) {
  if (asynchronous) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(^{
          return UIPasteboard.generalPasteboard;
        }),
        std::move(callback));
  } else {
    std::move(callback).Run(UIPasteboard.generalPasteboard);
  }
}
