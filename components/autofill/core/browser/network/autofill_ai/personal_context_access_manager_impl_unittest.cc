// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl_test_api.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/mock_personal_context_enablement_service.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using personal_context::ContextMemoryError;
using ::personal_context::MockPersonalContextEnablementService;
using ::personal_context::MockPersonalContextService;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

using RequestStatus = PersonalContextAccessManager::RequestStatus;

[[nodiscard]] auto HasAttributeWithValue(AttributeTypeName attribute_type_name,
                                         std::u16string value) {
  return Truly([=](const EntityInstance& entity) {
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(AttributeType(attribute_type_name));
    return attribute && attribute->GetCompleteInfo(/*app_locale=*/"") == value;
  });
}

[[nodiscard]] auto AmbientAutofillFetchRequestWithType(
    std::vector<personal_context::proto::EntityType> types) {
  return ResultOf(
      [](const google::protobuf::MessageLite& request) {
        return static_cast<const personal_context::proto::
                               ContextMemoryAmbientAutofillRequest&>(request)
            .requested_types();
      },
      ElementsAreArray(types));
}


class MockPersonalContextAccessManagerObserver
    : public PersonalContextAccessManager::Observer {
 public:
  MockPersonalContextAccessManagerObserver() = default;
  ~MockPersonalContextAccessManagerObserver() override = default;

  MOCK_METHOD(void,
              OnPrefetchAmbientAutofillContextComplete,
              (bool success),
              (override));
};

class PersonalContextAccessManagerImplTest : public testing::Test {
 public:
  PersonalContextAccessManagerImplTest() {
    ON_CALL(mock_enablement_service_, GetEnablementState)
        .WillByDefault(testing::Return(
            personal_context::PersonalContextEnablementState::kEnabled));
    observation_.Observe(&access_manager_);
  }
  ~PersonalContextAccessManagerImplTest() override = default;

  PersonalContextAccessManagerImpl& access_manager() { return access_manager_; }

  MockPersonalContextService& mock_personal_context_service() {
    return mock_personal_context_service_;
  }

  MockPersonalContextEnablementService& mock_enablement_service() {
    return mock_enablement_service_;
  }

  MockPersonalContextAccessManagerObserver& mock_observer() {
    return mock_observer_;
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  std::optional<EntityInstance> GetUnmaskedSpiiEntitySync(
      const EntityInstance::EntityId& id) {
    base::test::TestFuture<std::optional<EntityInstance>> future;
    access_manager().GetUnmaskedSpiiEntity(id, future.GetCallback());
    return future.Get();
  }

  void PrefetchAmbientAutofillContextSync(
      const std::vector<EntityType>& requested_types,
      const personal_context::proto::ContextMemoryAmbientAutofillResponse&
          response) {
    personal_context::proto::Any any_response;
    response.SerializeToString(any_response.mutable_value());

    std::vector<personal_context::proto::EntityType> proto_types;
    for (const auto& type : requested_types) {
      proto_types.push_back(
          AutofillEntityTypeToPersonalContextEntityType(type));
    }

    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
            AmbientAutofillFetchRequestWithType(proto_types), _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::ok(std::move(any_response)))));

    access_manager().PrefetchAmbientAutofillContext(requested_types);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAmbientAutofill};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockPersonalContextService mock_personal_context_service_;
  MockPersonalContextEnablementService mock_enablement_service_;
  PersonalContextAccessManagerImpl access_manager_{
      &mock_personal_context_service_, &mock_enablement_service_};
  MockPersonalContextAccessManagerObserver mock_observer_;
  base::ScopedObservation<PersonalContextAccessManagerImpl,
                          MockPersonalContextAccessManagerObserver>
      observation_{&mock_observer_};
};

// Tests that PrefetchAmbientAutofillContext successfully requests context from
// the backend, parses the returned entities, and caches them with their TTL.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextSuccess) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");
  entity->mutable_order()->set_merchant_name("Amazon");

  PrefetchAmbientAutofillContextSync(requested_types, expected_response);

  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
  EXPECT_THAT(access_manager().GetCachedEntities(),
              UnorderedElementsAre(AllOf(
                  Property(&EntityInstance::type,
                           Property(&EntityType::name, EntityTypeName::kOrder)),
                  HasAttributeWithValue(AttributeTypeName::kOrderId, u"12345"),
                  HasAttributeWithValue(AttributeTypeName::kOrderMerchantName,
                                        u"Amazon"))));

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EXPECT_EQ(cached[0].type(), EntityType(EntityTypeName::kOrder));

  base::optional_ref<const AttributeInstance> order_id_attr =
      cached[0].attribute(AttributeType(AttributeTypeName::kOrderId));
  ASSERT_TRUE(order_id_attr.has_value());
  EXPECT_EQ(order_id_attr->GetCompleteRawInfo(), u"12345");

  base::optional_ref<const AttributeInstance> merchant_attr =
      cached[0].attribute(AttributeType(AttributeTypeName::kOrderMerchantName));
  ASSERT_TRUE(merchant_attr.has_value());
  EXPECT_EQ(merchant_attr->GetCompleteRawInfo(), u"Amazon");
}

// Tests that PrefetchAmbientAutofillContext filters out and only requests
// entity types that are not currently cached.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextOnlyRequestsUncachedTypes) {
  // 1. First, cache Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_response;
  passport_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     passport_response);
  ASSERT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  // 2. Now call PrefetchAmbientAutofillContext for both Passport and Driver's
  // License. It should only request Driver's License.
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport),
      EntityType(EntityTypeName::kDriversLicense)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_drivers_license()->set_number("DL98765");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          // Only DRIVERS_LICENSE should be in the request, not PASSPORT.
          AmbientAutofillFetchRequestWithType(
              {personal_context::proto::EntityType::DRIVERS_LICENSE}),
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  // Both should now be cached.
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypeCached(
      EntityType(EntityTypeName::kDriversLicense)));
}

// Tests that PrefetchAmbientAutofillContext immediately returns and triggers
// no network requests when all requested entity types are already cached.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextAllCachedNoRequest) {
  // 1. Cache Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_response;
  passport_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     passport_response);
  ASSERT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  // 2. Call PrefetchAmbientAutofillContext for Passport.
  // No network request should be made.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};
  access_manager().PrefetchAmbientAutofillContext(requested_types);
}

// Tests that PrefetchAmbientAutofillContext does not cache anything or
// mark types as cached when the fetch context request fails.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextFailure) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::ContextMemoryError expected_error =
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure);

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
  EXPECT_THAT(access_manager().GetCachedEntities(), IsEmpty());
}

// Tests that `PrefetchAmbientAutofillContext` marks requested types as cached
// even when the response is empty.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextNegativeCaching) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder),
      EntityType(EntityTypeName::kPassport)};

  // Empty response.
  personal_context::proto::ContextMemoryAmbientAutofillResponse empty_response;
  PrefetchAmbientAutofillContextSync(requested_types, empty_response);

  // Both types should be marked as cached, but have no entities.
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_THAT(access_manager().GetCachedEntities(), IsEmpty());
}

// Tests that prefetched entities are cached with a 30-minute TTL, and that the
// TTL is tracked per entity type.
TEST_F(PersonalContextAccessManagerImplTest, CachePrefetchedEntities_TTL) {
  // 1. Cache Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_response;
  passport_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     passport_response);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_FALSE(access_manager().IsTypeCached(
      EntityType(EntityTypeName::kDriversLicense)));

  // Fast forward 15 minutes (Passport still valid).
  FastForwardBy(base::Minutes(15));
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  // 2. Cache DL at T+15.
  personal_context::proto::ContextMemoryAmbientAutofillResponse dl_response;
  dl_response.add_entities()->mutable_drivers_license()->set_number("DL987");
  PrefetchAmbientAutofillContextSync(
      {EntityType(EntityTypeName::kDriversLicense)}, dl_response);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypeCached(
      EntityType(EntityTypeName::kDriversLicense)));

  // Fast forward another 15 minutes (Total T+30). Passport should expire, DL
  // should be valid.
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypeCached(
      EntityType(EntityTypeName::kDriversLicense)));

  // Fast forward another 15 minutes (Total T+45). DL should expire.
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(access_manager().IsTypeCached(
      EntityType(EntityTypeName::kDriversLicense)));
}

// Tests that unmasked SPII entities are cached with a 1-minute TTL.
TEST_F(PersonalContextAccessManagerImplTest, CacheUnmaskedSpiiEntity_TTL) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);

  // Fast forward 30 seconds (still valid).
  FastForwardBy(base::Seconds(30));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);

  // Fast forward another 31 seconds (expired).
  FastForwardBy(base::Seconds(31));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), std::nullopt);
}

// Tests that resetting the cache for a type clears any existing cached entities
// of that type (useful for caching empty results).
TEST_F(PersonalContextAccessManagerImplTest, ResetCacheForType) {
  // Cache passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_response;
  passport_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     passport_response);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EntityInstance::EntityId passport_guid = cached[0].guid();

  // Reset cache (empty). Should clear passport.
  test_api(access_manager())
      .ResetCacheForType(EntityType(EntityTypeName::kPassport));
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_EQ(access_manager().GetCachedEntity(passport_guid), std::nullopt);
}

// Tests that GetCachedEntities returns only the prefetched (masked)
// entities and excludes any unmasked SPII entities.
TEST_F(PersonalContextAccessManagerImplTest, GetCachedEntities) {
  // 1. Cache prefetched (masked Passport and DL).
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_passport()->set_number("P123");
  response.add_entities()->mutable_drivers_license()->set_number("DL987");
  PrefetchAmbientAutofillContextSync(
      {EntityType(EntityTypeName::kPassport),
       EntityType(EntityTypeName::kDriversLicense)},
      response);

  // GetCachedEntities should return masked Passport and DL.
  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  EXPECT_EQ(cached.size(), 2u);

  // Now cache unmasked Passport.
  EntityInstance passport_unmasked = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport_unmasked);

  // GetCachedEntities should STILL return masked Passport and DL (size 2).
  EXPECT_EQ(access_manager().GetCachedEntities().size(), 2u);
}

// Tests that natural expiration of the prefetched cache also evicts any
// corresponding unmasked SPII entities, even if they haven't reached their
// individual TTL yet.
TEST_F(PersonalContextAccessManagerImplTest,
       CachePrefetchedEntities_ExpirationResetsUnmaskedCache) {
  // 1. Cache prefetched (masked) Passport at T=0.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_response;
  passport_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     passport_response);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EntityInstance::EntityId passport_guid = cached[0].guid();

  // 2. Fast forward 29.5 minutes.
  FastForwardBy(base::Minutes(29) + base::Seconds(30));

  // The prefetched cache is still valid (expires in 30 seconds).
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().GetCachedEntity(passport_guid).has_value());

  // 3. Cache unmasked SPII Passport at T=29.5.
  EntityInstance passport_unmasked = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  // Use the same GUID as the masked one to ensure they are linked.
  passport_unmasked = passport_unmasked.CopyWithNewEntityId(passport_guid);
  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport_unmasked);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), passport_unmasked);

  // 4. Fast forward 30 seconds (Total T+30). The prefetched cache expires.
  // This should also trigger the eviction of the unmasked SPII cache.
  FastForwardBy(base::Seconds(30));

  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_EQ(access_manager().GetCachedEntity(passport_guid), std::nullopt);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
}

// Tests that GetUnmaskedSpiiEntity returns the cached entity immediately
// without calling the service if it is already in the unmasked cache.
TEST_F(PersonalContextAccessManagerImplTest, GetUnmaskedSpiiEntity_CacheHit) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport);

  // No service call expected.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);
}

// Tests that GetUnmaskedSpiiEntity triggers a service call on cache miss
// (when the masked entity is prefetched), caches the unmasked result,
// and returns it.
TEST_F(PersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_CacheMiss_Success) {
  // 1. Prefetch (masked) Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     response);
  ASSERT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EntityInstance passport_masked = cached[0];

  // 2. Prepare unmasked response.
  personal_context::proto::FetchPiiEntitiesResponse expected_response;
  expected_response.add_entities()->mutable_passport()->set_number(
      "P123_UNMASKED");

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::ok(std::move(expected_response)))));

  // Call GetUnmaskedSpiiEntity.
  {
    std::optional<EntityInstance> result =
        GetUnmaskedSpiiEntitySync(passport_masked.guid());
    ASSERT_TRUE(result.has_value());
    // The result should be unmasked and have the same GUID.
    EXPECT_FALSE(result->IsMaskedEntity());
    EXPECT_EQ(result->guid(), passport_masked.guid());
    EXPECT_EQ(
        result->attribute(AttributeType(AttributeTypeName::kPassportNumber))
            ->GetCompleteRawInfo(),
        u"P123_UNMASKED");
  }

  // Verify that the unmasked passport is now cached in the unmasked cache by
  // calling `GetUnmaskedSpiiEntitySync` again and ensuring no service call
  // is made.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  {
    std::optional<EntityInstance> cached_result =
        GetUnmaskedSpiiEntitySync(passport_masked.guid());
    ASSERT_TRUE(cached_result.has_value());
    EXPECT_EQ(cached_result->guid(), passport_masked.guid());
    EXPECT_FALSE(cached_result->IsMaskedEntity());
  }
}

// Tests that `GetUnmaskedSpiiEntity` returns `std::nullopt` immediately
// if the requested entity is not even in the prefetched (masked) cache.
TEST_F(PersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_CacheMiss_NotPrefetched) {
  EntityInstance::EntityId unknown_id("unknown_id");

  // No service call expected because it's not prefetched.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(unknown_id), std::nullopt);
}

// Tests that `GetUnmaskedSpiiEntity` returns `std::nullopt` if the service call
// fails.
TEST_F(PersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_CacheMiss_ServiceFailure) {
  // 1. Prefetch (masked) Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     response);

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EntityInstance::EntityId passport_guid = cached[0].guid();

  using personal_context::ContextMemoryError;
  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::unexpected(expected_error))));

  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
}

// Tests that PrefetchAmbientAutofillContext is not executed if the
// kAutofillAmbientAutofill flag is disabled.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContext_FlagDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(features::kAutofillAmbientAutofill);

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
}

// Tests that PrefetchAmbientAutofillContext is not executed if the
// enablement state does not return an enabled state.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContext_EnablementDisabled) {
  EXPECT_CALL(mock_enablement_service(), GetEnablementState)
      .WillRepeatedly(
          testing::Return(personal_context::PersonalContextEnablementState::
                              kDisabledNotEligible));

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
}

// Tests that PrefetchAmbientAutofillContext is executed if the
// enablement state is kEnabledShouldShowNotice.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContext_EnabledShouldShowNotice) {
  EXPECT_CALL(mock_enablement_service(), GetEnablementState)
      .WillRepeatedly(
          testing::Return(personal_context::PersonalContextEnablementState::
                              kEnabledShouldShowNotice));

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  auto create_expected_response = []() -> personal_context::proto::Any {
    personal_context::proto::ContextMemoryAmbientAutofillResponse
        expected_response;
    personal_context::proto::Entity* entity = expected_response.add_entities();
    entity->mutable_order()->set_order_id("12345");
    entity->mutable_order()->set_merchant_name("Amazon");

    personal_context::proto::Any any_response;
    expected_response.SerializeToString(any_response.mutable_value());
    return any_response;
  };

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(create_expected_response()))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
}

// Tests that when OnEnablementStateChanged is called with a disabled state, all
// caches are wiped.
TEST_F(PersonalContextAccessManagerImplTest, WipeCachesOnDisablement) {
  // 1. Cache prefetched (masked) passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_response;
  passport_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     passport_response);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EntityInstance::EntityId passport_guid = cached[0].guid();
  EntityInstance passport = cached[0];

  // 2. Call OnEnablementStateChanged with an ENABLED state. Caches should not
  // be wiped.
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::
          kEnabledShouldShowNotice);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_EQ(access_manager().GetCachedEntity(passport_guid), passport);

  // 3. Call OnEnablementStateChanged with a DISABLED state. Caches should be
  // wiped.
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::
          kDisabledViaPersonalIntelligenceInAutofillToggle);
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kPassport)));
  EXPECT_EQ(access_manager().GetCachedEntity(passport_guid), std::nullopt);
}

// Tests that a pending request blocks subsequent requests for the same type.
TEST_F(PersonalContextAccessManagerImplTest, PendingRequestBlocksSubsequent) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  base::test::TestFuture<personal_context::FetchContextCallback> future;
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(WithArg<3>(InvokeFuture(future)));

  // First request should trigger FetchContext.
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  ASSERT_TRUE(future.IsReady());

  // Second request for the same type should NOT trigger FetchContext.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchAmbientAutofillContext(requested_types);

  // It isn't cachet yet.
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));

  // Resolve the first request.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());
  future.Take().Run(
      personal_context::FetchContextResult(base::ok(std::move(any_response))));

  // Now it is cached.
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
}

// Tests that failed requests trigger exponential backoff.
TEST_F(PersonalContextAccessManagerImplTest, FailureTriggersBackoff) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    // 1. First failure.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::unexpected(expected_error))));
    EXPECT_CALL(check, Call("1. First failure"));

    // 2. Immediate retry should be blocked by backoff (1s delay).
    EXPECT_CALL(check, Call("2. Immediate retry"));

    // 3. Fast forward 500ms (still blocked).
    EXPECT_CALL(check, Call("3. Fast forward 500ms"));

    // 4. Fast forward another 500ms (total 1s, backoff expired).
    // Second failure.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::unexpected(expected_error))));
    EXPECT_CALL(check, Call("4. Second failure"));

    // 5. Immediate retry should be blocked by backoff (2s delay now).
    EXPECT_CALL(check, Call("5. Immediate retry"));

    // 6. Fast forward 1.5s (still blocked).
    EXPECT_CALL(check, Call("6. Fast forward 1.5s"));

    // 7. Fast forward another 500ms (total 2s, backoff expired).
    // This time it succeeds.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::ok(std::move(any_response)))));
    EXPECT_CALL(check, Call("7. Success"));

    // 9. Success resets failure count.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::unexpected(expected_error))));
    EXPECT_CALL(check, Call("9. Reset success"));
  }

  // 1. First failure.
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
  check.Call("1. First failure");

  // 2. Immediate retry should be blocked by backoff (1s delay).
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  check.Call("2. Immediate retry");

  // 3. Fast forward 500ms (still blocked).
  FastForwardBy(base::Milliseconds(500));
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  check.Call("3. Fast forward 500ms");

  // 4. Fast forward another 500ms (total 1s, backoff expired).
  FastForwardBy(base::Milliseconds(500));
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  check.Call("4. Second failure");

  // 5. Immediate retry should be blocked by backoff (2s delay now).
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  check.Call("5. Immediate retry");

  // 6. Fast forward 1.5s (still blocked).
  FastForwardBy(base::Milliseconds(1500));
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  check.Call("6. Fast forward 1.5s");

  // 7. Fast forward another 500ms (total 2s, backoff expired).
  FastForwardBy(base::Milliseconds(500));
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  EXPECT_TRUE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));
  check.Call("7. Success");

  // 8. Expire the cache (30 mins).
  FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(
      access_manager().IsTypeCached(EntityType(EntityTypeName::kOrder)));

  // 9. Request again, should succeed immediately because failure count was
  // reset on success.
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  check.Call("9. Reset success");
}

// Tests that the prefetch status transitions correctly (`kNotStarted` ->
// `kPending` -> `kSuccess` -> `kNotStarted`) and the observer is notified
// with success = true when a prefetch request succeeds.
TEST_F(PersonalContextAccessManagerImplTest, PrefetchStatusAndObserverSuccess) {
  const EntityType order_type = EntityType(EntityTypeName::kOrder);

  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kNotStarted);

  base::test::TestFuture<personal_context::FetchContextCallback> future;
  EXPECT_CALL(mock_personal_context_service(), FetchContext)
      .WillOnce(WithArg<3>(InvokeFuture(future)));

  // 1. Start prefetch. Status should transition to `kPending`.
  access_manager().PrefetchAmbientAutofillContext({order_type});
  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kPending);

  // 2. Resolve request successfully. Status should transition to `kSuccess`,
  // and observer should be notified with success = true.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(mock_observer(), OnPrefetchAmbientAutofillContextComplete(true));
  future.Take().Run(
      personal_context::FetchContextResult(base::ok(std::move(any_response))));

  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kSuccess);

  // 3. Fast forward 30 minutes (TTL expires). Status should transition back to
  // `kNotStarted`.
  FastForwardBy(base::Minutes(30));
  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kNotStarted);
}

// Tests that the prefetch status transitions correctly (`kNotStarted` ->
// `kPending` -> `kFailure` -> `kNotStarted`) and the observer is notified
// with success = false when a prefetch request fails.
TEST_F(PersonalContextAccessManagerImplTest, PrefetchStatusAndObserverFailure) {
  const EntityType order_type = EntityType(EntityTypeName::kOrder);

  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kNotStarted);

  base::test::TestFuture<personal_context::FetchContextCallback> future;
  EXPECT_CALL(mock_personal_context_service(), FetchContext)
      .WillOnce(WithArg<3>(InvokeFuture(future)));

  // 1. Start prefetch. Status should transition to `kPending`.
  access_manager().PrefetchAmbientAutofillContext({order_type});
  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kPending);

  // 2. Resolve request with failure. Status should transition to `kFailure`,
  // and observer should be notified with success = false.
  EXPECT_CALL(mock_observer(), OnPrefetchAmbientAutofillContextComplete(false));
  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);
  future.Take().Run(
      personal_context::FetchContextResult(base::unexpected(expected_error)));

  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kFailure);

  // 3. Wipe caches. Status should transition back to `kNotStarted`.
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::
          kDisabledViaPersonalIntelligenceInAutofillToggle);
  EXPECT_EQ(
      access_manager().GetPrefetchAmbientAutofillStatusByEntityType(order_type),
      RequestStatus::kNotStarted);
}

// Tests that calling PrefetchAmbientAutofillContext when all types are cached
// indeed notifies the observer synchronously.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchWhenAlreadyCachedNotifiesObserver) {
  MockPersonalContextAccessManagerObserver observer;
  access_manager().AddObserver(&observer);

  // 1. Cache Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchAmbientAutofillContextSync({EntityType(EntityTypeName::kPassport)},
                                     response);

  // 2. Call Prefetch again. Expect observer to be notified synchronously.
  EXPECT_CALL(observer, OnPrefetchAmbientAutofillContextComplete(true));
  access_manager().PrefetchAmbientAutofillContext(
      {EntityType(EntityTypeName::kPassport)});
}

}  // namespace
}  // namespace autofill
