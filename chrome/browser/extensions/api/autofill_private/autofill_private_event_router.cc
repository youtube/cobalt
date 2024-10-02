// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
namespace {
template <class T>
base::Value::List ToValueList(const std::vector<T>& values) {
  base::Value::List list;
  list.reserve(values.size());
  for (const auto& value : values) {
    list.Append(value.ToValue());
  }
  return list;
}
}  // namespace

AutofillPrivateEventRouter::AutofillPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  event_router_ = EventRouter::Get(context_);
  if (!event_router_)
    return;

  personal_data_ = autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  if (!personal_data_)
    return;

  personal_data_->AddObserver(this);
}

void AutofillPrivateEventRouter::Shutdown() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

void AutofillPrivateEventRouter::OnPersonalDataChanged() {
  BroadcastCurrentData();
}

void AutofillPrivateEventRouter::OnPersonalDataSyncStateChanged() {
  BroadcastCurrentData();
}

void AutofillPrivateEventRouter::BroadcastCurrentData() {
  // Ignore any updates before data is loaded. This can happen in tests.
  if (!(personal_data_ && personal_data_->IsDataLoaded()))
    return;

  autofill_util::AddressEntryList addressList =
      extensions::autofill_util::GenerateAddressList(*personal_data_);

  autofill_util::CreditCardEntryList creditCardList =
      extensions::autofill_util::GenerateCreditCardList(*personal_data_);

  autofill_util::IbanEntryList ibanList =
      extensions::autofill_util::GenerateIbanList(*personal_data_);

  absl::optional<api::autofill_private::AccountInfo> account_info =
      extensions::autofill_util::GetAccountInfo(*personal_data_);

  base::Value::List args;
  args.Append(ToValueList(addressList));
  args.Append(ToValueList(creditCardList));
  args.Append(ToValueList(ibanList));
  if (account_info.has_value()) {
    args.Append(account_info->ToValue());
  }

  std::unique_ptr<Event> extension_event(
      new Event(events::AUTOFILL_PRIVATE_ON_PERSONAL_DATA_CHANGED,
                api::autofill_private::OnPersonalDataChanged::kEventName,
                std::move(args)));

  event_router_->BroadcastEvent(std::move(extension_event));
}

AutofillPrivateEventRouter* AutofillPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new AutofillPrivateEventRouter(context);
}

}  // namespace extensions
