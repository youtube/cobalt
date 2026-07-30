// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_H_
#define CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/declarative_performance_observer.mojom.h"
#include "third_party/blink/public/mojom/timing/declarative_performance_observer.mojom.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;
class StoragePartition;

// Observes and buffers performance timeline entries (navigation timing,
// visibility state changes, and performance marks) for a single committed
// document.
//
// Created per-document when a navigation commits with a valid
// Performance-Observer policy. Buffers entries in memory up to a session quota
// (or session end) and queues reporting beacons via the NetworkContext. Relies
// on DeclarativePerformanceObserverCoordinator to forward tab-scoped
// WebContentsObserver lifecycle updates (visibility flips, BFCache
// transitions).
class CONTENT_EXPORT DeclarativePerformanceObserver
    : public DocumentUserData<DeclarativePerformanceObserver>,
      public blink::mojom::DeclarativePerformanceObserverHost {
 public:
  ~DeclarativePerformanceObserver() override;

  DeclarativePerformanceObserver(const DeclarativePerformanceObserver&) =
      delete;
  DeclarativePerformanceObserver& operator=(
      const DeclarativePerformanceObserver&) = delete;

  // Called by DeclarativePerformanceObserverCoordinator:
  void OnVisibilityChanged(Visibility visibility);
  void OnFrameDeleted();
  void OnEnterBFCache();
  void OnDidFinishNavigation(NavigationHandle* navigation_handle);
  void OnPrerenderActivation(NavigationHandle* navigation_handle);

  // blink::mojom::DeclarativePerformanceObserverHost:
  void DidObservePerformanceEntries(
      std::vector<blink::mojom::DeclarativePerformanceEntryPtr> entries)
      override;

  static void RecordEarlyNavigationFailure(NavigationHandle* handle,
                                           StoragePartition* partition,
                                           int net_error);

  static void Bind(
      RenderFrameHost* rfh,
      mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
          receiver);

  void SetStoragePartitionForTesting(  // IN-TEST
      StoragePartition* storage_partition);

 private:
  friend class DocumentUserData<DeclarativePerformanceObserver>;
  DeclarativePerformanceObserver(RenderFrameHost* rfh,
                                 NavigationHandle* navigation_handle);

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
          receiver);

  void AddEntryToBuffer(base::DictValue entry);
  void FlushMetrics();
  void AppendSessionEndEntry();
  void OnEarlyFailureReportsTaken(base::ListValue reports);

  std::string reporting_endpoint_;
  base::flat_set<network::mojom::PerformanceEntryType> enabled_types_;
  std::optional<base::flat_set<std::string>> include_user_timing_;
  base::ListValue buffered_entries_;
  bool started_in_foreground_ = false;
  bool is_session_ended_ = false;
  base::TimeTicks navigation_start_;
  GURL committed_url_;
  net::NetworkAnonymizationKey network_anonymization_key_;
  base::UnguessableToken reporting_source_;
  raw_ptr<StoragePartition> storage_partition_for_testing_ = nullptr;

  mojo::Receiver<blink::mojom::DeclarativePerformanceObserverHost> receiver_{
      this};

  base::WeakPtrFactory<DeclarativePerformanceObserver> weak_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_H_
