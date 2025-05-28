// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIPS_DIPS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIPS_DIPS_SERVICE_IMPL_H_

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/dips/dips_storage.h"
#include "content/browser/dips/dips_utils.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/dips_redirect_info.h"
#include "content/public/browser/dips_service.h"

namespace content {

class BrowserContext;
class BrowserContextImpl;
class PersistentRepeatingTimer;

// When DIPS moves to //content, BtmServiceImpl will *not* be exposed in the
// Content API. Only other code in //content (such as the DIPS implementation)
// will be allowed to access it.
class CONTENT_EXPORT BtmServiceImpl : public BtmService {
 public:
  using RecordBounceCallback = base::RepeatingCallback<void(
      const GURL& url,
      bool has_3pc_exception,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback)>;

  BtmServiceImpl(base::PassKey<BrowserContextImpl>, BrowserContext* context);
  ~BtmServiceImpl() override;

  static BtmServiceImpl* Get(BrowserContext* context);

  base::SequenceBound<BtmStorage>* storage() { return &storage_; }

  void RecordBounceForTesting(
      const GURL& url,
      bool has_3pc_exception,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback) {
    RecordBounce(url, has_3pc_exception, final_url, time, stateful,
                 stateful_bounce_callback);
  }

  BtmCookieMode GetCookieMode() const;

  void RemoveEvents(const base::Time& delete_begin,
                    const base::Time& delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const BtmEventRemovalType type);

  // This allows for deletion of state for sites deemed eligible when evaluated
  // with no grace period.
  void DeleteEligibleSitesImmediately(DeletedSitesCallback callback) override;

  void HandleRedirectChain(
      std::vector<BtmRedirectInfoPtr> redirects,
      BtmRedirectChainInfoPtr chain,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback);

  void RecordUserActivationForTesting(const GURL& url) override;

  void DidSiteHaveUserActivationSince(
      const GURL& url,
      base::Time bound,
      CheckUserActivationCallback callback) const override;

  // This allows unit-testing the metrics emitted by HandleRedirect() without
  // instantiating BtmService.
  static void HandleRedirectForTesting(const BtmRedirectInfo& redirect,
                                       const BtmRedirectChainInfo& chain,
                                       RecordBounceCallback callback) {
    HandleRedirect(redirect, chain, callback,
                   base::BindRepeating([](const GURL& final_url) {}));
  }

  void SetStorageClockForTesting(base::Clock* clock) {
    DCHECK(storage_);
    storage_.AsyncCall(&BtmStorage::SetClockForTesting).WithArgs(clock);
  }

  void OnTimerFiredForTesting() { OnTimerFired(); }
  void WaitForFileDeletionCompleteForTesting() {
    wait_for_file_deletion_.Run();
  }

  void AddObserver(Observer* observer) override;
  void RemoveObserver(const Observer* observer) override;

  void AddOpenSite(const std::string& site) {
    if (open_sites_.contains(site)) {
      open_sites_.at(site)++;
    } else {
      open_sites_.insert({site, 1});
    }
  }

  void RemoveOpenSite(const std::string& site) {
    CHECK(open_sites_.contains(site));
    if (open_sites_.contains(site)) {
      open_sites_.at(site)--;
      if (open_sites_.at(site) == 0) {
        open_sites_.erase(site);
      }
    }
  }

  // Notify Observers that a stateful bounce took place in `web_contents`.
  void NotifyStatefulBounce(WebContents* web_contents);

 private:
  std::unique_ptr<PersistentRepeatingTimer> CreateTimer();

  void GotState(
      std::vector<BtmRedirectInfoPtr> redirects,
      BtmRedirectChainInfoPtr chain,
      size_t index,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback,
      const BtmState url_state);
  void RecordBounce(
      const GURL& url,
      bool has_3pc_exception,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback);
  static void HandleRedirect(
      const BtmRedirectInfo& redirect,
      const BtmRedirectChainInfo& chain,
      RecordBounceCallback callback,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback);

  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();

  void OnTimerFired();
  void DeleteBtmEligibleState(DeletedSitesCallback callback,
                              std::vector<std::string> sites_to_clear);
  void RunDeletionTaskOnUIThread(std::vector<std::string> sites_to_clear,
                                 base::OnceClosure callback);

  // BtmService overrides:
  void RecordBrowserSignIn(std::string_view domain) override;

  base::RunLoop wait_for_file_deletion_;
  raw_ptr<BrowserContext> browser_context_;
  // The persisted timer controlling how often incidental state is cleared.
  // This timer is null if the DIPS feature isn't enabled with a valid TimeDelta
  // given for its `timer_delay` parameter.
  // See base/time/time_delta_from_string.h for how that param should be given.
  std::unique_ptr<PersistentRepeatingTimer> repeating_timer_;
  base::SequenceBound<BtmStorage> storage_;
  base::ObserverList<Observer> observers_;

  std::map<std::string, int> open_sites_;

  base::WeakPtrFactory<BtmServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIPS_DIPS_SERVICE_IMPL_H_
