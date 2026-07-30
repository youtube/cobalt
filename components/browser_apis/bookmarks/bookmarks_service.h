// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_SERVICE_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_SERVICE_H_

#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace bookmarks_api {

class BookmarksService : public mojom::BookmarksServiceDirectReturnStub {
 public:
  ~BookmarksService() override = default;

  // Mojo handling:
  virtual void Accept(
      mojo::PendingReceiver<mojom::BookmarksService> receiver) = 0;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_SERVICE_H_
