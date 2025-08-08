// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_condition.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source_enum.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_running_status_enum.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_routersource_routersourceenum.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

namespace {

blink::KURL DefaultBaseUrl() {
  return blink::KURL("https://www.example.com/test/base/url");
}

blink::SafeUrlPattern DefaultStringUrlPattern() {
  blink::SafeUrlPattern url_pattern;
  {
    liburlpattern::Part part;
    part.modifier = liburlpattern::Modifier::kNone;
    part.type = liburlpattern::PartType::kFixed;
    part.value = "https";
    url_pattern.protocol.emplace_back(part);
  }
  {
    liburlpattern::Part part;
    part.modifier = liburlpattern::Modifier::kNone;
    part.type = liburlpattern::PartType::kFixed;
    part.value = "www.example.com";
    url_pattern.hostname.emplace_back(part);
  }
  {
    liburlpattern::Part part;
    part.modifier = liburlpattern::Modifier::kNone;
    part.type = liburlpattern::PartType::kFixed;
    part.value = "/test/base/";
    url_pattern.pathname.emplace_back(part);
  }
  return url_pattern;
}

blink::SafeUrlPattern DefaultURLPatternInitUrlPattern() {
  blink::SafeUrlPattern url_pattern;

  liburlpattern::Part part;
  part.modifier = liburlpattern::Modifier::kNone;
  part.type = liburlpattern::PartType::kFullWildcard;
  part.name = "0";

  url_pattern.protocol.push_back(part);
  url_pattern.username.push_back(part);
  url_pattern.password.push_back(part);
  url_pattern.hostname.push_back(part);
  url_pattern.port.push_back(part);
  url_pattern.search.push_back(part);
  url_pattern.hash.push_back(part);

  return url_pattern;
}

TEST(ServiceWorkerRouterTypeConverterTest, Basic) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern = DefaultStringUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeUrlPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.pathname = parse_result.value().PartList();
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, BasicURLPatternInit) {
  constexpr const char kFakeProtoPattern[] = "https";
  constexpr const char kFakeHostPattern[] = "example.com";
  constexpr const char kFakePathPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  blink::URLPatternInit* init = blink::URLPatternInit::Create();
  init->setProtocol(kFakeProtoPattern);
  init->setHostname(kFakeHostPattern);
  init->setPathname(kFakePathPattern);
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(init));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern =
      DefaultURLPatternInitUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeProtoPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.protocol = parse_result.value().PartList();
  }
  {
    auto parse_result = liburlpattern::Parse(
        kFakeHostPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.hostname = parse_result.value().PartList();
  }
  {
    auto parse_result = liburlpattern::Parse(
        kFakePathPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.pathname = parse_result.value().PartList();
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, URLPatternInitWithEmptyPathname) {
  constexpr const char kFakeProtoPattern[] = "https";
  constexpr const char kFakeHostPattern[] = "example.com";
  constexpr const char kFakePathPattern[] = "";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  blink::URLPatternInit* init = blink::URLPatternInit::Create();
  init->setProtocol(kFakeProtoPattern);
  init->setHostname(kFakeHostPattern);
  init->setPathname(kFakePathPattern);
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(init));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern =
      DefaultURLPatternInitUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeProtoPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.protocol = parse_result.value().PartList();
  }
  {
    auto parse_result = liburlpattern::Parse(
        kFakeHostPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.hostname = parse_result.value().PartList();
  }
  // An empty string must be translated to an empty vector.
  expected_url_pattern.pathname = {};
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest,
     EmptyUrlPatternShouldBeBaseURLPattern) {
  constexpr const char kFakeUrlPattern[] = "";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  expected_rule.condition = blink::ServiceWorkerRouterCondition::WithUrlPattern(
      DefaultStringUrlPattern());
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest,
     EmptyUrlPatternAndEmptyBaseURLShouldThrowException) {
  constexpr const char kFakeUrlPattern[] = "";
  const KURL kFakeBaseUrl("");
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, kFakeBaseUrl,
                                               scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_FALSE(blink_rule.has_value());
}

TEST(ServiceWorkerRouterTypeConverterTest, RegexpUrlPatternShouldBeNullopt) {
  auto verify = [](const WTF::String& test_url_pattern) {
    auto* idl_rule = blink::RouterRule::Create();
    auto* idl_condition = blink::RouterCondition::Create();
    idl_condition->setUrlPattern(
        MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
            test_url_pattern));
    idl_rule->setCondition(idl_condition);
    idl_rule->setSource(
        MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
            blink::V8RouterSourceEnum(
                blink::V8RouterSourceEnum::Enum::kNetwork)));

    V8TestingScope scope;
    auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                                 scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
    EXPECT_FALSE(blink_rule.has_value());
  };
  verify("/fake/(\\\\d+)");
  verify("://fake(\\\\d+).com/");
}

TEST(ServiceWorkerRouterTypeConverterTest, Race) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kRaceNetworkAndFetchHandler)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern = DefaultStringUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeUrlPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.pathname = parse_result.value().PartList();
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kRace;
  expected_source.race_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, FetchEvent) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kFetchEvent)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern = DefaultStringUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeUrlPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.pathname = parse_result.value().PartList();
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kFetchEvent;
  expected_source.fetch_event_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, Request) {
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setRequestMethod("FakeRequestMethod");
  idl_condition->setRequestMode(blink::V8RequestMode::Enum::kNavigate);
  idl_condition->setRequestDestination(
      blink::V8RequestDestination::Enum::kDocument);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterRequestCondition expected_request;
  expected_request.method = "FakeRequestMethod";
  expected_request.mode = network::mojom::RequestMode::kNavigate;
  expected_request.destination = network::mojom::RequestDestination::kDocument;
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithRequest(expected_request);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, RequestMethodNormalize) {
  auto validate_normalize = [](const WTF::String& input,
                               const std::string& expected) {
    auto* idl_rule = blink::RouterRule::Create();
    auto* idl_condition = blink::RouterCondition::Create();
    idl_condition->setRequestMethod(input);
    idl_rule->setCondition(idl_condition);
    idl_rule->setSource(
        MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
            blink::V8RouterSourceEnum(
                blink::V8RouterSourceEnum::Enum::kNetwork)));

    blink::ServiceWorkerRouterRule expected_rule;
    blink::ServiceWorkerRouterRequestCondition expected_request;
    expected_request.method = expected;
    expected_rule.condition =
        blink::ServiceWorkerRouterCondition::WithRequest(expected_request);
    blink::ServiceWorkerRouterSource expected_source;
    expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
    expected_source.network_source.emplace();
    expected_rule.sources.emplace_back(expected_source);

    V8TestingScope scope;
    auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                                 scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());
    EXPECT_TRUE(blink_rule.has_value());
    EXPECT_EQ(expected_rule, *blink_rule);
  };
  validate_normalize("DeLeTe", "DELETE");
  validate_normalize("gEt", "GET");
  validate_normalize("HeAd", "HEAD");
  validate_normalize("oPtIoNs", "OPTIONS");
  validate_normalize("PoSt", "POST");
  validate_normalize("pUt", "PUT");
  validate_normalize("anythingElse", "anythingElse");
}

TEST(ServiceWorkerRouterTypeConverterTest, RunningStatus) {
  auto verify =
      [](blink::V8RunningStatusEnum::Enum idl_status,
         blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum
             blink_status) {
        auto* idl_rule = blink::RouterRule::Create();
        auto* idl_condition = blink::RouterCondition::Create();
        idl_condition->setRunningStatus(idl_status);
        idl_rule->setCondition(idl_condition);
        idl_rule->setSource(
            MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
                blink::V8RouterSourceEnum(
                    blink::V8RouterSourceEnum::Enum::kNetwork)));

        blink::ServiceWorkerRouterRule expected_rule;
        blink::ServiceWorkerRouterRunningStatusCondition expected_status;
        expected_status.status = blink_status;
        expected_rule.condition =
            blink::ServiceWorkerRouterCondition::WithRunningStatus(
                expected_status);
        blink::ServiceWorkerRouterSource expected_source;
        expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
        expected_source.network_source.emplace();
        expected_rule.sources.emplace_back(expected_source);

        V8TestingScope scope;
        auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                                     scope.GetExceptionState());
        EXPECT_FALSE(scope.GetExceptionState().HadException());
        EXPECT_TRUE(blink_rule.has_value());
        EXPECT_EQ(expected_rule, *blink_rule);
      };
  verify(blink::V8RunningStatusEnum::Enum::kRunning,
         blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
             kRunning);
  verify(blink::V8RunningStatusEnum::Enum::kNotRunning,
         blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
             kNotRunning);
}

TEST(ServiceWorkerRouterTypeConverterTest, EmptyOrConditionShouldBeAllowed) {
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  HeapVector<Member<RouterCondition>> idl_or_conditions;
  idl_condition->setOrConditions(idl_or_conditions);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithOrCondition({});

  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, OrConditionWithMultipleElements) {
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  HeapVector<Member<RouterCondition>> idl_or_conditions;
  {
    auto* element = blink::RouterCondition::Create();
    element->setRequestMethod("FakeRequestMethod");
    element->setRequestMode(blink::V8RequestMode::Enum::kNavigate);
    element->setRequestDestination(
        blink::V8RequestDestination::Enum::kDocument);
    idl_or_conditions.push_back(element);
  }
  {
    auto* element = blink::RouterCondition::Create();
    element->setRunningStatus(blink::V8RunningStatusEnum::Enum::kRunning);
    idl_or_conditions.push_back(element);
  }
  idl_condition->setOrConditions(idl_or_conditions);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterOrCondition expected_or_condition;
  {
    blink::ServiceWorkerRouterRequestCondition expected_request;
    expected_request.method = "FakeRequestMethod";
    expected_request.mode = network::mojom::RequestMode::kNavigate;
    expected_request.destination =
        network::mojom::RequestDestination::kDocument;
    expected_or_condition.conditions.push_back(
        blink::ServiceWorkerRouterCondition::WithRequest(expected_request));
  }
  {
    blink::ServiceWorkerRouterRunningStatusCondition expected_status;
    expected_status.status = blink::ServiceWorkerRouterRunningStatusCondition::
        RunningStatusEnum::kRunning;
    expected_or_condition.conditions.push_back(
        blink::ServiceWorkerRouterCondition::WithRunningStatus(
            expected_status));
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithOrCondition(
          expected_or_condition);

  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, NestedOrCondition) {
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  HeapVector<Member<RouterCondition>> idl_outer_or;
  {
    auto* idl_inner = blink::RouterCondition::Create();
    HeapVector<Member<RouterCondition>> idl_inner_or;
    idl_inner->setOrConditions(idl_inner_or);
    idl_outer_or.push_back(idl_inner);
  }
  idl_condition->setOrConditions(idl_outer_or);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterOrCondition expected_outer;
  {
    expected_outer.conditions.emplace_back(
        blink::ServiceWorkerRouterCondition::WithOrCondition({}));
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithOrCondition(expected_outer);

  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest,
     OrConditionCombinedWithOthersShouldThrowException) {
  auto* idl_rule = blink::RouterRule::Create();
  const KURL kFakeBaseUrl("");
  auto* idl_condition = blink::RouterCondition::Create();
  HeapVector<Member<RouterCondition>> idl_or_conditions;
  idl_condition->setOrConditions(idl_or_conditions);
  // Set another rule
  idl_condition->setRunningStatus(blink::V8RunningStatusEnum::Enum::kRunning);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(
              blink::V8RouterSourceEnum::Enum::kNetwork)));

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, kFakeBaseUrl,
                                               scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_FALSE(blink_rule.has_value());
}

// TODO(crbug.com/1490445): Add tests to limit depth of condition nests

TEST(ServiceWorkerRouterTypeConverterTest, Cache) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          blink::V8RouterSourceEnum(blink::V8RouterSourceEnum::Enum::kCache)));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern = DefaultStringUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeUrlPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.pathname = parse_result.value().PartList();
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kCache;
  expected_source.cache_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, CacheName) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(
      MakeGarbageCollected<blink::V8UnionURLPatternInitOrUSVString>(
          kFakeUrlPattern));
  idl_rule->setCondition(idl_condition);
  auto* idl_source = blink::RouterSource::Create();
  idl_source->setCacheName("cache_name");
  idl_rule->setSource(
      MakeGarbageCollected<blink::V8UnionRouterSourceOrRouterSourceEnum>(
          idl_source));

  blink::ServiceWorkerRouterRule expected_rule;
  blink::SafeUrlPattern expected_url_pattern = DefaultStringUrlPattern();
  {
    auto parse_result = liburlpattern::Parse(
        kFakeUrlPattern,
        [](base::StringPiece input) { return std::string(input); });
    ASSERT_TRUE(parse_result.ok());
    expected_url_pattern.pathname = parse_result.value().PartList();
  }
  expected_rule.condition =
      blink::ServiceWorkerRouterCondition::WithUrlPattern(expected_url_pattern);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::Type::kCache;
  blink::ServiceWorkerRouterCacheSource cache_source;
  cache_source.cache_name = "cache_name";
  expected_source.cache_source = std::move(cache_source);
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule = ConvertV8RouterRuleToBlink(idl_rule, DefaultBaseUrl(),
                                               scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

}  // namespace

}  // namespace blink
