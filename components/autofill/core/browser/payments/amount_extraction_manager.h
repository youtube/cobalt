// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace autofill {
class AutofillDriver;
class BrowserAutofillManager;
struct SuggestionsContext;
}  // namespace autofill

namespace autofill::payments {

// Owned by `BrowserAutofillManager`. This class manages the flow of the
// checkout amount extraction. The amount extraction flow starts from
// `BrowserAutofillManager`, which will call `AmountExtractionManager` to check
// whether a search should be triggered. If eligible, `AmountExtractionManager`
// will ask the main frame `AutofillDriver` to search for the checkout amount
// from the main frame DOM tree because it's where the final checkout amount
// lives. Since the search may take some time, it does not block credit card
// suggestion from being shown on UI because the IPC call between the browser
// process and renderer process is non-blocking. Once the result is passed back,
// `AmountExtractionManager` will pass the result to `BnplManager` and let it
// check whether this value is eligible for a Buy Now, Pay Later(BNPL) offer. If
// so, a new BNPL chip and the existing credit card suggestions will be shown on
// UI together. If not, it does nothing.
class AmountExtractionManager {
 public:
  explicit AmountExtractionManager(BrowserAutofillManager* autofill_manager);
  AmountExtractionManager(const AmountExtractionManager& other) = delete;
  AmountExtractionManager& operator=(const AmountExtractionManager& other) =
      delete;
  virtual ~AmountExtractionManager();

  // This function attempts to convert a string representation of a monetary
  // value in dollars into a uint64_t by parsing it as a double and multiplying
  // the result by 1,000,000. It assumes the input uses a decimal point ('.') as
  // the separator for fractional values (not a decimal comma). The function
  // only supports English-style monetary representations like $, USD, etc.
  // Multiplication by 1,000,000 is done to represent the monetary value in
  // micro-units (1 dollar = 1,000,000 micro-units), which is commonly used in
  // systems that require high precision for financial calculations.
  static std::optional<uint64_t> MaybeParseAmountToMonetaryMicroUnits(
      const std::string& amount);

  // Decide whether amount extraction should be triggered.
  virtual bool ShouldTriggerAmountExtraction(const SuggestionsContext& context,
                                             bool should_suppress_suggestions,
                                             bool has_suggestions) const;

  // Trigger the search for the final checkout amount from the DOM of the
  // current page.
  virtual void TriggerCheckoutAmountExtraction();

  void SetSearchRequestPendingForTesting(bool search_request_pending);

 private:
  // Check whether the host of the checkout webpage exists in the amount
  // extraction allowlists.
  bool IsUrlEligibleForAmountExtraction() const;

  // Invoked after the amount extraction process completes.
  // `extracted_amount` provides the extracted amount upon success and an
  // empty string upon failure. `search_request_start_timestamp` is the time
  // when TriggerCheckoutAmountExtraction is called.
  void OnCheckoutAmountReceived(base::TimeTicks search_request_start_timestamp,
                                const std::string& extracted_amount);

  // Get the driver associated with the main frame as the final checkout amount
  // is on the main frame.
  AutofillDriver* GetMainFrameDriver();

  // The owning BrowserAutofillManager.
  raw_ref<BrowserAutofillManager> autofill_manager_;

  // Indicates whether there is an amount search ongoing or not. If set, do not
  // trigger the search. It gets reset to false once the search is done. This is
  // to avoid re-triggering amount extraction multiple times during an ongoing
  // search.
  bool search_request_pending_ = false;

  base::WeakPtrFactory<AmountExtractionManager> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_
