// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/browser_exposed_child_interfaces.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/child/child_histogram_fetcher_impl.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/tracing/public/cpp/traced_process.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace content {

void ExposeChildInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
  binders->Add<mojom::ChildHistogramFetcherFactory>(
      base::BindRepeating(&ChildHistogramFetcherFactoryImpl::Create),
      io_task_runner);
  binders->Add<tracing::mojom::TracedProcess>(
      base::BindRepeating(&tracing::TracedProcess::OnTracedProcessRequest),
      base::SequencedTaskRunner::GetCurrentDefault());

  GetContentClient()->ExposeInterfacesToBrowser(io_task_runner, binders);
}

}  // namespace content
