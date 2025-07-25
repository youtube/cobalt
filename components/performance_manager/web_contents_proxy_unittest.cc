// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/web_contents_proxy.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

class WebContentsProxyTest : public PerformanceManagerTestHarness {
 public:
  WebContentsProxyTest() {}

  WebContentsProxyTest(const WebContentsProxyTest&) = delete;
  WebContentsProxyTest& operator=(const WebContentsProxyTest&) = delete;

  ~WebContentsProxyTest() override {}

  void GetContentsViaProxy(bool delete_before_deref);
};

TEST_F(WebContentsProxyTest, EndToEnd) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = PerformanceManagerTabHelper::FromWebContents(contents.get());
  auto* page_node = helper->primary_page_node();
  content::WebContents* proxy_contents = nullptr;

  auto deref_proxy = base::BindLambdaForTesting(
      [&proxy_contents](const WebContentsProxy& proxy,
                        base::OnceClosure quit_loop) {
        proxy_contents = proxy.Get();
        std::move(quit_loop).Run();
      });

  // Bounce over to the PM sequence, retrieve the proxy, bounce back to the UI
  // thread, dereference it if possible, and save the returned contents. To be
  // fair, it's entirely valid to grab the weak pointer directly on the UI
  // thread, as the lifetime of the page node is managed their and the property
  // being accessed is thread safe. However, this test aims to simulate what
  // would happen with a policy message being posted from the graph.
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&deref_proxy, page_node, quit_loop = run_loop.QuitClosure()]() {
              content::GetUIThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(deref_proxy, page_node->contents_proxy(),
                                 std::move(quit_loop)));
            }));
    run_loop.Run();

    EXPECT_EQ(contents.get(), proxy_contents);
  }

  // Run the same test, but this time destroy the contents prior to
  // dereferencing the proxy.
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindLambdaForTesting([&contents, &deref_proxy, page_node,
                                    quit_loop = run_loop.QuitClosure()]() {
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindLambdaForTesting([&contents]() { contents.reset(); }));
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(deref_proxy, page_node->contents_proxy(),
                             std::move(quit_loop)));
        }));
    run_loop.Run();

    // The contents was destroyed on the UI thread prior to dereferencing the
    // proxy, so it should return nullptr.
    EXPECT_FALSE(proxy_contents);
  }
}

}  // namespace performance_manager
