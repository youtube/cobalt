// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_GENERIC_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_GENERIC_H_

#include "base/time/time.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// An implementation of ClipboardRecentContent that uses
// ui/base/clipboard/clipboard.h
// and hence works on all platforms for which Clipboard is implemented.
// (This includes all platforms Chrome runs on except iOS.)
// Note that on some platforms Clipboard may not implement the necessary
// functions for this provider to function.  In those cases, it will not do
// anything.
class ClipboardRecentContentGeneric : public ClipboardRecentContent {
 public:
  ClipboardRecentContentGeneric();

  ClipboardRecentContentGeneric(const ClipboardRecentContentGeneric&) = delete;
  ClipboardRecentContentGeneric& operator=(
      const ClipboardRecentContentGeneric&) = delete;

  ~ClipboardRecentContentGeneric() override;

  // ClipboardRecentContent implementation.
  absl::optional<GURL> GetRecentURLFromClipboard() override;
  absl::optional<std::u16string> GetRecentTextFromClipboard() override;
  absl::optional<std::set<ClipboardContentType>>
  GetCachedClipboardContentTypes() override;
  void GetRecentImageFromClipboard(GetRecentImageCallback callback) override;
  bool HasRecentImageFromClipboard() override;
  void HasRecentContentFromClipboard(std::set<ClipboardContentType> types,
                                     HasDataCallback callback) override;
  void GetRecentURLFromClipboard(GetRecentURLCallback callback) override;
  void GetRecentTextFromClipboard(GetRecentTextCallback callback) override;
  base::TimeDelta GetClipboardContentAge() const override;
  void SuppressClipboardContent() override;
  void ClearClipboardContent() override;

 private:
  // Returns true if the URL is appropriate to be suggested.
  static bool IsAppropriateSuggestion(const GURL& url);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_GENERIC_H_
