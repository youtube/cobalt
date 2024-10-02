// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_shopping_service.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace commerce {

// static
std::unique_ptr<KeyedService> MockShoppingService::Build() {
  return std::make_unique<MockShoppingService>();
}

MockShoppingService::MockShoppingService()
    : commerce::ShoppingService("us",
                                "en-us",
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr) {
  // Set up some defaults so tests don't have to explicitly set up each.
  SetResponseForGetProductInfoForUrl(absl::nullopt);
  SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::map<int64_t, ProductInfo>());
  ON_CALL(*this, GetMaxProductBookmarkUpdatesPerBatch)
      .WillByDefault(testing::Return(30));
  SetResponseForGetMerchantInfoForUrl(absl::nullopt);
  SetSubscribeCallbackValue(true);
  SetUnsubscribeCallbackValue(true);
  SetIsSubscribedCallbackValue(true);
  SetGetAllSubscriptionsCallbackValue(std::vector<CommerceSubscription>());
  SetIsShoppingListEligible(true);
  SetIsClusterIdTrackedByUserResponse(true);
  SetIsMerchantViewerEnabled(true);
}

MockShoppingService::~MockShoppingService() = default;

void MockShoppingService::SetResponseForGetProductInfoForUrl(
    absl::optional<commerce::ProductInfo> product_info) {
  ON_CALL(*this, GetProductInfoForUrl)
      .WillByDefault([product_info](const GURL& url,
                                    commerce::ProductInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), url, product_info));
      });

  ON_CALL(*this, GetAvailableProductInfoForUrl)
      .WillByDefault(testing::Return(product_info));
}

void MockShoppingService::SetResponsesForGetUpdatedProductInfoForBookmarks(
    std::map<int64_t, ProductInfo> bookmark_updates) {
  ON_CALL(*this, GetUpdatedProductInfoForBookmarks)
      .WillByDefault(
          [bookmark_updates = std::move(bookmark_updates)](
              const std::vector<int64_t>& bookmark_ids,
              BookmarkProductInfoUpdatedCallback info_updated_callback) {
            for (auto id : bookmark_ids) {
              auto it = bookmark_updates.find(id);

              if (it == bookmark_updates.end()) {
                continue;
              }

              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(info_updated_callback, it->first,
                                            GURL(""), it->second));
            }
          });
}

void MockShoppingService::SetResponseForGetMerchantInfoForUrl(
    absl::optional<commerce::MerchantInfo> merchant_info) {
  ON_CALL(*this, GetMerchantInfoForUrl)
      .WillByDefault([merchant_info](const GURL& url,
                                     MerchantInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), url, merchant_info));
      });
}

void MockShoppingService::SetSubscribeCallbackValue(
    bool subscribe_should_succeed) {
  ON_CALL(*this, Subscribe)
      .WillByDefault(
          [subscribe_should_succeed](
              std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
              base::OnceCallback<void(bool)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), subscribe_should_succeed));
          });
}

void MockShoppingService::SetUnsubscribeCallbackValue(
    bool unsubscribe_should_succeed) {
  ON_CALL(*this, Unsubscribe)
      .WillByDefault(
          [unsubscribe_should_succeed](
              std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
              base::OnceCallback<void(bool)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          unsubscribe_should_succeed));
          });
}

void MockShoppingService::SetIsSubscribedCallbackValue(bool is_subscribed) {
  ON_CALL(*this, IsSubscribed)
      .WillByDefault([is_subscribed](CommerceSubscription subscription,
                                     base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), is_subscribed));
      });
  ON_CALL(*this, IsSubscribedFromCache)
      .WillByDefault(testing::Return(is_subscribed));
}

void MockShoppingService::SetGetAllSubscriptionsCallbackValue(
    std::vector<CommerceSubscription> subscriptions) {
  ON_CALL(*this, GetAllSubscriptions)
      .WillByDefault([subs = std::move(subscriptions)](
                         SubscriptionType type,
                         base::OnceCallback<void(
                             std::vector<CommerceSubscription>)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(subs)));
      });
}

void MockShoppingService::SetIsShoppingListEligible(bool eligible) {
  ON_CALL(*this, IsShoppingListEligible)
      .WillByDefault(testing::Return(eligible));
}

void MockShoppingService::SetIsClusterIdTrackedByUserResponse(bool is_tracked) {
  ON_CALL(*this, IsClusterIdTrackedByUser)
      .WillByDefault([is_tracked](uint64_t cluster_id,
                                  base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), is_tracked));
      });
}

void MockShoppingService::SetIsMerchantViewerEnabled(bool is_enabled) {
  ON_CALL(*this, IsMerchantViewerEnabled)
      .WillByDefault(testing::Return(is_enabled));
}

}  // namespace commerce
