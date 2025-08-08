// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BACKGROUND_RESOURCE_FETCH_ASSETS_H_
#define CONTENT_RENDERER_BACKGROUND_RESOURCE_FETCH_ASSETS_H_

#include "third_party/blink/public/platform/web_background_resource_fetch_assets.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace content {

// An implementation of WebBackgroundResourceFetchAssets.
class BackgroundResourceFetchAssets
    : public blink::WebBackgroundResourceFetchAssets {
 public:
  BackgroundResourceFetchAssets(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  BackgroundResourceFetchAssets(const BackgroundResourceFetchAssets&) = delete;
  BackgroundResourceFetchAssets& operator=(
      const BackgroundResourceFetchAssets&) = delete;

  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunner() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetLoaderFactory() override;

 private:
  ~BackgroundResourceFetchAssets() override;

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_loader_factory_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_BACKGROUND_RESOURCE_FETCH_ASSETS_H_
