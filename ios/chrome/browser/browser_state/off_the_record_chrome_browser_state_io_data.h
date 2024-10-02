// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_OFF_THE_RECORD_CHROME_BROWSER_STATE_IO_DATA_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_OFF_THE_RECORD_CHROME_BROWSER_STATE_IO_DATA_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_io_data.h"
#include "ios/chrome/browser/net/net_types.h"

class ChromeBrowserState;
class IOSChromeURLRequestContextGetter;

// OffTheRecordChromeBrowserState owns a
// OffTheRecordChromeBrowserStateIOData::Handle, which holds a reference to the
// OffTheRecordChromeBrowserStateIOData.
// OffTheRecordChromeBrowserStateIOData is intended to own all the objects owned
// by OffTheRecordChromeBrowserState which live on the IO thread, such as, but
// not limited to, network objects like CookieMonster, HttpTransactionFactory,
// etc.
// OffTheRecordChromeBrowserStateIOData is owned by the
// OffTheRecordChromeBrowserState and OffTheRecordChromeBrowserStateIOData's
// IOSChromeURLRequestContexts. When all of them go away, then
// ChromeBrowserStateIOData will be deleted. Note that the
// OffTheRecordChromeBrowserStateIOData will typically outlive the browser state
// it is "owned" by, so it's important for OffTheRecordChromeBrowserStateIOData
// not to hold any references to the browser state beyond what's used by
// LazyParams (which should be deleted after lazy initialization).
class OffTheRecordChromeBrowserStateIOData : public ChromeBrowserStateIOData {
 public:
  class Handle {
   public:
    explicit Handle(ChromeBrowserState* browser_state);

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    ~Handle();

    scoped_refptr<IOSChromeURLRequestContextGetter>
    CreateMainRequestContextGetter(ProtocolHandlerMap* protocol_handlers) const;

    // Clears the HTTP cache associated with the incognito browser state.
    void DoomIncognitoCache();

    ChromeBrowserStateIOData* io_data() const;

   private:
    // Lazily initialize ProfileParams. We do this on the calls to
    // Get*RequestContextGetter(), so we only initialize ProfileParams right
    // before posting a task to the IO thread to start using them. This prevents
    // objects that are supposed to be deleted on the IO thread, but are created
    // on the UI thread from being unnecessarily initialized.
    void LazyInitialize() const;

    // Collect references to context getters in reverse order, i.e. last item
    // will be main request getter. This list is passed to `io_data_`
    // for invalidation on IO thread.
    std::unique_ptr<IOSChromeURLRequestContextGetterVector>
    GetAllContextGetters();

    // The getters will be invalidated on the IO thread before
    // ProfileIOData instance is deleted.
    mutable scoped_refptr<IOSChromeURLRequestContextGetter>
        main_request_context_getter_;
    OffTheRecordChromeBrowserStateIOData* const io_data_;

    ChromeBrowserState* const browser_state_;

    mutable bool initialized_;
  };

  OffTheRecordChromeBrowserStateIOData(
      const OffTheRecordChromeBrowserStateIOData&) = delete;
  OffTheRecordChromeBrowserStateIOData& operator=(
      const OffTheRecordChromeBrowserStateIOData&) = delete;

 private:
  friend class base::RefCountedThreadSafe<OffTheRecordChromeBrowserStateIOData>;

  OffTheRecordChromeBrowserStateIOData();
  ~OffTheRecordChromeBrowserStateIOData() override;

  void InitializeInternal(net::URLRequestContextBuilder* context_builder,
                          ProfileParams* profile_params) const override;

  // Server bound certificates and cookies are persisted to the disk on iOS.
  base::FilePath cookie_path_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_OFF_THE_RECORD_CHROME_BROWSER_STATE_IO_DATA_H_
