// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_reporter.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_context.h"
#include "url/gurl.h"

namespace content {

namespace {

class UnderlyingNetworkContext : public network::TestNetworkContext {
 public:
  explicit UnderlyingNetworkContext(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void QueueSignedExchangeReport(
      network::mojom::SignedExchangeReportPtr report,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool called() const { return called_; }

 private:
  bool called_ = false;
  base::OnceClosure quit_closure_;
};

}  // namespace

class SignedExchangeReporterBrowserTest : public ContentBrowserTest {
 public:
  SignedExchangeReporterBrowserTest() {
    feature_list_.InitAndEnableFeature(network::features::kReporting);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// SignedExchangeReporter is created when the outer signed-exchange response is
// received, but the report itself is queued only after asynchronous work (e.g.
// fetching the certificate, or decoding the inner body) completes. The frame
// may have committed a cross-StoragePartition document in the interim. Verify
// that the report is queued on the StoragePartition that performed the load.
IN_PROC_BROWSER_TEST_F(SignedExchangeReporterBrowserTest,
                       ReportQueuedOnOriginatingStoragePartition) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Map b.com to a non-default StoragePartition.
  CustomStoragePartitionBrowserClient modified_client(GURL("http://b.com/"));

  // Navigate the main frame to a.com (default StoragePartition).
  const GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), a_url));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  StoragePartition* partition_a = rfh_a->GetStoragePartition();
  FrameTreeNode* frame_tree_node = rfh_a->frame_tree_node();

  // Create the reporter as the prefetch / navigation handler would on receipt
  // of the outer response.
  auto response_head = network::mojom::URLResponseHead::New();
  std::unique_ptr<SignedExchangeReporter> reporter =
      SignedExchangeReporter::MaybeCreate(
          embedded_test_server()->GetURL("a.com", "/test.sxg"), a_url.spec(),
          *response_head, net::NetworkAnonymizationKey(),
          frame_tree_node->frame_tree_node_id());
  ASSERT_TRUE(reporter);

  // Swap the FrameTreeNode's current RenderFrameHost to a document in the
  // non-default StoragePartition while the reporter is still alive.
  const GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), b_url));
  RenderFrameHostImpl* rfh_b = web_contents()->GetPrimaryMainFrame();
  StoragePartition* partition_b = rfh_b->GetStoragePartition();
  ASSERT_NE(partition_a, partition_b);
  ASSERT_EQ(frame_tree_node, rfh_b->frame_tree_node());

  // Set up mock NetworkContexts for both partitions to observe where the
  // report is queued.
  base::RunLoop run_loop;
  UnderlyingNetworkContext context_a(run_loop.QuitClosure());
  mojo::Receiver<network::mojom::NetworkContext> receiver_a(&context_a);
  partition_a->SetNetworkContextForTesting(
      receiver_a.BindNewPipeAndPassRemote());

  UnderlyingNetworkContext context_b(base::NullCallback());
  mojo::Receiver<network::mojom::NetworkContext> receiver_b(&context_b);
  partition_b->SetNetworkContextForTesting(
      receiver_b.BindNewPipeAndPassRemote());

  // Finish the load and wait for the report to be queued.
  reporter->ReportLoadResultAndFinish(SignedExchangeLoadResult::kSuccess);
  run_loop.Run();

  EXPECT_TRUE(context_a.called());
  EXPECT_FALSE(context_b.called());
}

}  // namespace content
