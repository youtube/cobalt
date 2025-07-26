// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_

namespace content {

// The interface to control prefetch resources associated with this.
// If the handle is destructed, it will notify PrefetchService that the
// corresponding PrefetchContainer is no longer needed. PrefetchService will try
// to release relevant resources by its own decision with best-effort.
class PrefetchHandle {
 public:
  PrefetchHandle() = default;
  virtual ~PrefetchHandle() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_
