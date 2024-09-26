// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_HANDLER_H_
#define CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_partition_key.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class WebContents;
class BrowserContext;

// This handler parses the Clear-Site-Data header and executes the clearing
// of browsing data. The resource load is delayed until the header is parsed
// and, if valid, until the browsing data are deleted. See the W3C working draft
// at https://w3c.github.io/webappsec-clear-site-data/.
class CONTENT_EXPORT ClearSiteDataHandler {
 public:
  // Stores and outputs console messages.
  class CONTENT_EXPORT ConsoleMessagesDelegate {
   public:
    struct Message {
      GURL url;
      std::string text;
      blink::mojom::ConsoleMessageLevel level;
    };

    using OutputFormattedMessageFunction =
        base::RepeatingCallback<void(WebContents*,
                                     blink::mojom::ConsoleMessageLevel,
                                     const std::string&)>;

    ConsoleMessagesDelegate();
    virtual ~ConsoleMessagesDelegate();

    // Logs a |text| message from |url| with |level|.
    virtual void AddMessage(const GURL& url,
                            const std::string& text,
                            blink::mojom::ConsoleMessageLevel level);

    // Outputs stored messages to the console of WebContents identified by
    // |web_contents_getter|.
    virtual void OutputMessages(
        const base::RepeatingCallback<WebContents*()>& web_contents_getter);

    const std::vector<Message>& GetMessagesForTesting() const {
      return messages_;
    }

   protected:
    void SetOutputFormattedMessageFunctionForTesting(
        const OutputFormattedMessageFunction& function);

   private:
    std::vector<Message> messages_;
    OutputFormattedMessageFunction output_formatted_message_function_;
  };

  ClearSiteDataHandler(const ClearSiteDataHandler&) = delete;
  ClearSiteDataHandler& operator=(const ClearSiteDataHandler&) = delete;

  // |header_value| is the string value of the 'Clear-Site-Data' header. This
  // method calls ParseHeader() to parse it, and then ExecuteClearingTask() if
  // applicable.
  static void HandleHeader(
      base::RepeatingCallback<BrowserContext*()> browser_context_getter,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
      const absl::optional<blink::StorageKey>& storage_key,
      bool partitioned_state_allowed_only,
      base::OnceClosure callback);

  // Exposes ParseHeader() publicly for testing.
  static bool ParseHeaderForTesting(
      const std::string& header,
      bool* clear_cookies,
      bool* clear_storage,
      bool* clear_cache,
      std::set<std::string>* storage_buckets_to_remove,
      ConsoleMessagesDelegate* delegate,
      const GURL& current_url);

 protected:
  ClearSiteDataHandler(
      base::RepeatingCallback<BrowserContext*()> browser_context_getter,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
      const absl::optional<blink::StorageKey>& storage_key,
      bool partitioned_state_allowed_only,
      base::OnceClosure callback,
      std::unique_ptr<ConsoleMessagesDelegate> delegate);
  virtual ~ClearSiteDataHandler();

  // Calls |HandleHeaderImpl| to handle headers, and output console message if
  // not deferred. Returns |true| if the request was deferred.
  bool HandleHeaderAndOutputConsoleMessages();

  // Handles headers and maybe execute clearing task. Returns |true| if the
  // request was deferred.
  bool Run();

  // Parses the value of the 'Clear-Site-Data' header and outputs whether
  // the header requests to |clear_cookies|, |clear_storage|, and |clear_cache|.
  // The |delegate| will be filled with messages to be output in the console,
  // prepended by the |current_url|. Returns true if parsing was successful.
  static bool ParseHeader(const std::string& header,
                          bool* clear_cookies,
                          bool* clear_storage,
                          bool* clear_cache,
                          std::set<std::string>* storage_buckets_to_remove,
                          ConsoleMessagesDelegate* delegate,
                          const GURL& current_url);

  // Executes the clearing task. Can be overridden for testing.
  virtual void ExecuteClearingTask(
      const url::Origin& origin,
      bool clear_cookies,
      bool clear_storage,
      bool clear_cache,
      const std::set<std::string>& storage_buckets_to_remove,
      base::OnceClosure callback);

  // Signals that a parsing and deletion task was finished.
  // |clearing_started| is the time when the last clearing operation started.
  // Used when clearing finishes to compute the duration.
  static void TaskFinished(
      base::TimeTicks clearing_started,
      std::unique_ptr<ConsoleMessagesDelegate> delegate,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      base::OnceClosure callback);

  // Outputs the console messages in the |delegate_|.
  void OutputConsoleMessages();

  // Run the callback to resume loading. No clearing actions were conducted.
  void RunCallbackNotDeferred();

  const GURL& GetURLForTesting();

  const absl::optional<net::CookiePartitionKey> CookiePartitionKeyForTesting()
      const {
    return cookie_partition_key_;
  }

  const absl::optional<blink::StorageKey> StorageKeyForTesting() const {
    return storage_key_;
  }

  bool PartitionedStateOnlyForTesting() const {
    return partitioned_state_allowed_only_;
  }

 private:
  // Required to clear the data.
  base::RepeatingCallback<BrowserContext*()> browser_context_getter_;
  base::RepeatingCallback<WebContents*()> web_contents_getter_;

  // Target URL whose data will be cleared.
  const GURL url_;

  // Raw string value of the 'Clear-Site-Data' header.
  const std::string header_value_;

  // Load flags of the current request, used to check cookie policies.
  const int load_flags_;

  // The cookie partition key for which we need to clear partitioned cookies
  // when we receive the Clear-Site-Data header.
  const absl::optional<net::CookiePartitionKey> cookie_partition_key_;

  // The storage key for which we need to clear partitioned storage when we
  // receive the Clear-Site-Data header.
  const absl::optional<blink::StorageKey> storage_key_;

  // If third-party cookie blocking is enabled and applies to the response that
  // sent Clear-Site-Data.
  const bool partitioned_state_allowed_only_;

  // Used to notify that the clearing has completed. Callers could resuming
  // loading after this point.
  base::OnceClosure callback_;

  // The delegate that stores and outputs console messages.
  std::unique_ptr<ConsoleMessagesDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_HANDLER_H_
