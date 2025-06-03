// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_H_

#include <map>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/net/net_types.h"
#include "ios/web/public/browser_state.h"
#include "net/url_request/url_request_job_factory.h"

class BrowserStatePolicyConnector;
class ChromeBrowserStateIOData;
class PrefProxyConfigTracker;
class PrefService;
class TestChromeBrowserState;
class TestChromeBrowserStateManager;

namespace base {
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace web {
class WebUIIOS;
}

namespace policy {
class UserCloudPolicyManager;
}

enum class ChromeBrowserStateType {
  REGULAR_BROWSER_STATE,
  INCOGNITO_BROWSER_STATE,
};

// This class is a Chrome-specific extension of the BrowserState interface.
class ChromeBrowserState : public web::BrowserState {
 public:
  ChromeBrowserState(const ChromeBrowserState&) = delete;
  ChromeBrowserState& operator=(const ChromeBrowserState&) = delete;

  ~ChromeBrowserState() override;

  // Returns the ChromeBrowserState corresponding to the given BrowserState.
  static ChromeBrowserState* FromBrowserState(BrowserState* browser_state);

  // Returns the ChromeBrowserState corresponding to the given WebUIIOS.
  static ChromeBrowserState* FromWebUIIOS(web::WebUIIOS* web_ui);

  // Returns sequenced task runner where browser state dependent I/O
  // operations should be performed.
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner();

  // Returns the original "recording" ChromeBrowserState. This method returns
  // `this` if the ChromeBrowserState is not incognito.
  virtual ChromeBrowserState* GetOriginalChromeBrowserState() = 0;

  // Returns true if the ChromeBrowserState is off-the-record or if the
  // associated off-the-record browser state has been created.
  // Calling this method does not create the off-the-record browser state if it
  // does not already exist.
  virtual bool HasOffTheRecordChromeBrowserState() const = 0;

  // Returns the incognito version of this ChromeBrowserState. The returned
  // ChromeBrowserState instance is owned by this ChromeBrowserState instance.
  // WARNING: This will create the OffTheRecord ChromeBrowserState if it
  // doesn't already exist.
  virtual ChromeBrowserState* GetOffTheRecordChromeBrowserState() = 0;

  // Destroys the OffTheRecord ChromeBrowserState that is associated with this
  // ChromeBrowserState, if one exists.
  virtual void DestroyOffTheRecordChromeBrowserState() = 0;

  // Retrieves a pointer to the BrowserStatePolicyConnector that manages policy
  // for this BrowserState. May return nullptr if policy is disabled.
  virtual BrowserStatePolicyConnector* GetPolicyConnector() = 0;

  // Returns a pointer to the UserCloudPolicyManager that is a facade for the
  // user cloud policy system.
  virtual policy::UserCloudPolicyManager* GetUserCloudPolicyManager() = 0;

  // Retrieves a pointer to the PrefService that manages the preferences.
  virtual PrefService* GetPrefs();

  // Retrieves a pointer to the PrefService that manages the preferences as
  // a sync_preferences::PrefServiceSyncable.
  virtual sync_preferences::PrefServiceSyncable* GetSyncablePrefs() = 0;

  // Allows access to ChromeBrowserStateIOData without going through
  // ResourceContext that is not compiled on iOS. This method must be called on
  // UI thread, but the returned object must only be accessed on the IO thread.
  virtual ChromeBrowserStateIOData* GetIOData() = 0;

  // Deletes all network related data since `time`. It deletes transport
  // security state since `time` and it also deletes HttpServerProperties data.
  // Works asynchronously, however if the `completion` callback is non-null, it
  // will be posted on the UI thread once the removal process completes.
  // Be aware that theoretically it is possible that `completion` will be
  // invoked after the BrowserState instance has been destroyed.
  virtual void ClearNetworkingHistorySince(base::Time time,
                                           base::OnceClosure completion) = 0;

  // Returns an identifier of the browser state for debugging.
  std::string GetDebugName();

  // Returns the helper object that provides the proxy configuration service
  // access to the the proxy configuration possibly defined by preferences.
  virtual PrefProxyConfigTracker* GetProxyConfigTracker() = 0;

  // Creates the main net::URLRequestContextGetter that will be returned by
  // GetRequestContext(). Should only be called once.
  virtual net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) = 0;

  // Returns a weak pointer to the current instance.
  virtual base::WeakPtr<ChromeBrowserState> AsWeakPtr() = 0;

  // Returns the path where the off-the-record BrowserState data is stored.
  // If the BrowserState is off-the-record, this is equal to GetStatePath().
  base::FilePath GetOffTheRecordStatePath() const;

  // web::BrowserState
  base::FilePath GetStatePath() const final;
  net::URLRequestContextGetter* GetRequestContext() final;
  void UpdateCorsExemptHeader(
      network::mojom::NetworkContextParams* params) final;

 protected:
  explicit ChromeBrowserState(
      const base::FilePath& state_path,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

 private:
  friend class ::TestChromeBrowserState;
  friend class ::TestChromeBrowserStateManager;

  base::FilePath const state_path_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_CHROME_BROWSER_STATE_H_
