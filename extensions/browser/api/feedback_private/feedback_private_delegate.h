// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "extensions/common/api/feedback_private.h"

#include <memory>
#include <string>

#include "build/chromeos_buildflags.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace feedback {
class FeedbackUploader;
}  // namespace feedback

namespace system_logs {
class SystemLogsSource;
}  // namespace system_logs

namespace extensions {

using FetchExtraLogsCallback =
    base::OnceCallback<void(scoped_refptr<feedback::FeedbackData>)>;

// Delegate class for embedder-specific chrome.feedbackPrivate behavior.
class FeedbackPrivateDelegate {
 public:
  virtual ~FeedbackPrivateDelegate() = default;

  // Returns a dictionary of localized strings for the feedback component
  // extension.
  // Set |from_crash| to customize strings when the feedback UI was initiated
  // from a "sad tab" crash.
  virtual base::Value::Dict GetStrings(content::BrowserContext* browser_context,
                                       bool from_crash) const = 0;

  virtual void FetchSystemInformation(
      content::BrowserContext* context,
      system_logs::SysLogsFetcherCallback callback) const = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Creates a SystemLogsSource for the given type of log file.
  virtual std::unique_ptr<system_logs::SystemLogsSource> CreateSingleLogSource(
      api::feedback_private::LogSource source_type) const = 0;

  // Gets logs that aren't passed to the sendFeedback function, but should be
  // included in the feedback report. These currently consist of the Intel Wi-Fi
  // debug logs (if they exist).
  // Modifies |feedback_data| and passes it on to |callback|.
  virtual void FetchExtraLogs(
      scoped_refptr<feedback::FeedbackData> feedback_data,
      FetchExtraLogsCallback callback) const = 0;

  // Returns the type of the landing page which is shown to the user when the
  // report is successfully sent.
  virtual api::feedback_private::LandingPageType GetLandingPageType(
      const feedback::FeedbackData& feedback_data) const = 0;

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets Lacros histograms in zip compressed format which will be attached
  // as a file in unified feedback report.
  virtual void GetLacrosHistograms(GetHistogramsCallback callback) = 0;
#endif

  // Returns the normalized email address of the signed-in user associated with
  // the browser context, if any.
  virtual std::string GetSignedInUserEmail(
      content::BrowserContext* context) const = 0;

  // Called if sending the feedback report was delayed.
  virtual void NotifyFeedbackDelayed() const = 0;

  // Returns the uploader associated with |context| which is used to upload
  // feedback reports to the feedback server.
  virtual feedback::FeedbackUploader* GetFeedbackUploaderForContext(
      content::BrowserContext* context) const = 0;

  // Opens the feedback report window.
  virtual void OpenFeedback(
      content::BrowserContext* context,
      api::feedback_private::FeedbackSource source) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_DELEGATE_H_
