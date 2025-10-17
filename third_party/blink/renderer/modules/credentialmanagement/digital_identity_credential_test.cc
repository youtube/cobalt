// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_create_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_get_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_request_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

using testing::_;
using testing::AllOf;
using testing::SizeIs;
using testing::WithArg;

// `arg` must be of type
// std::vector<blink::mojom::DigitalCredentialGetRequestPtr>
MATCHER(FirstRequestDataContainsString, "") {
  return arg[0]->data->is_str();
}
MATCHER(FirstRequestDataContainsValue, "") {
  return arg[0]->data->is_value();
}

// Mock mojom::DigitalIdentityRequest which succeeds and returns "token".
class MockDigitalIdentityRequest : public mojom::DigitalIdentityRequest {
 public:
  MockDigitalIdentityRequest() = default;

  MockDigitalIdentityRequest(const MockDigitalIdentityRequest&) = delete;
  MockDigitalIdentityRequest& operator=(const MockDigitalIdentityRequest&) =
      delete;

  void Bind(mojo::PendingReceiver<mojom::DigitalIdentityRequest> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD(
      void,
      Get,
      (std::vector<blink::mojom::DigitalCredentialGetRequestPtr> requests,
       blink::mojom::GetRequestFormat format,
       GetCallback callback),
      (override));

  void Create(
      std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests,
      CreateCallback callback) override {
    // Return a Value::String instead of a Dict because V8ValueConverterForTest
    // doesn't support converting Dict.
    std::move(callback).Run(mojom::RequestDigitalIdentityStatus::kSuccess,
                            "protocol", base::Value("token"));
  }

  void Abort() override {}

 private:
  mojo::Receiver<mojom::DigitalIdentityRequest> receiver_{this};
};

CredentialRequestOptions* CreateLegacyGetOptionsWithProviders(
    const HeapVector<Member<IdentityRequestProvider>>& providers) {
  DigitalCredentialRequestOptions* digital_credential_request =
      DigitalCredentialRequestOptions::Create();
  digital_credential_request->setProviders(providers);
  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  options->setDigital(digital_credential_request);
  return options;
}

CredentialRequestOptions* CreateLegacyGetValidOptions() {
  IdentityRequestProvider* identity_provider =
      IdentityRequestProvider::Create();
  identity_provider->setProtocol("openid4vp");
  identity_provider->setRequest(
      MakeGarbageCollected<V8UnionObjectOrString>(String("request")));
  HeapVector<Member<IdentityRequestProvider>> identity_providers;
  identity_providers.push_back(identity_provider);
  return CreateLegacyGetOptionsWithProviders(identity_providers);
}

CredentialRequestOptions* CreateGetOptionsWithRequests(
    const HeapVector<Member<DigitalCredentialGetRequest>>& requests) {
  DigitalCredentialRequestOptions* digital_credential_request =
      DigitalCredentialRequestOptions::Create();
  digital_credential_request->setRequests(requests);
  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  options->setDigital(digital_credential_request);
  return options;
}

CredentialRequestOptions* CreateValidGetOptions(ScriptState* script_state) {
  v8::Local<v8::Context> context = script_state->GetContext();
  DigitalCredentialGetRequest* request = DigitalCredentialGetRequest::Create();
  request->setProtocol("openid4vp");
  v8::Local<v8::Object> request_data =
      v8::Object::New(script_state->GetIsolate());
  v8::Maybe<bool> maybe =
      request_data->Set(context, V8String(script_state->GetIsolate(), "key"),
                        V8String(script_state->GetIsolate(), "value"));
  CHECK(maybe.IsJust() || maybe.FromJust());

  request->setData(MakeGarbageCollected<V8UnionObjectOrString>(
      ScriptObject(script_state->GetIsolate(), request_data)));
  HeapVector<Member<DigitalCredentialGetRequest>> requests;
  requests.push_back(request);
  return CreateGetOptionsWithRequests(requests);
}

CredentialCreationOptions* CreateValidCreateOptions() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  DigitalCredentialCreateRequest* request =
      DigitalCredentialCreateRequest::Create();
  request->setProtocol("openid4vci");
  request->setData(ScriptObject(isolate, v8::Object::New(isolate)));
  HeapVector<Member<DigitalCredentialCreateRequest>> requests;
  requests.push_back(request);

  DigitalCredentialCreationOptions* digital_credential_creation_options =
      DigitalCredentialCreationOptions::Create();
  digital_credential_creation_options->setRequests(requests);

  CredentialCreationOptions* options = CredentialCreationOptions::Create();
  options->setDigital(digital_credential_creation_options);
  return options;
}

}  // namespace

class DigitalIdentityCredentialTest : public PageTestBase {
 public:
  DigitalIdentityCredentialTest() = default;
  ~DigitalIdentityCredentialTest() override = default;

  DigitalIdentityCredentialTest(const DigitalIdentityCredentialTest&) = delete;
  DigitalIdentityCredentialTest& operator=(
      const DigitalIdentityCredentialTest&) = delete;

  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();
    ON_CALL(mock_request_, Get)
        .WillByDefault(
            WithArg<2>([](mojom::DigitalIdentityRequest::GetCallback callback) {
              // Return a Value::String instead of a Dict because
              // V8ValueConverterForTest doesn't support converting Dict.
              std::move(callback).Run(
                  mojom::RequestDigitalIdentityStatus::kSuccess, "protocol",
                  base::Value("token"));
            }));
  }

  MockDigitalIdentityRequest* mock_request() { return &mock_request_; }

 private:
  MockDigitalIdentityRequest mock_request_;
};

// Test that navigator.credentials.get() increments the feature use counter when
// one of the identity providers is a digital identity credential using the
// legacy request format.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialUseCounterForLegacyRequestFormat) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request())));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  // Legacy requests contains a string, and hence are passed over to the
  // request as string.
  EXPECT_CALL(*mock_request(),
              Get(AllOf(SizeIs(1), FirstRequestDataContainsString()),
                  mojom::GetRequestFormat::kLegacy, _));
  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateLegacyGetValidOptions(), context.GetExceptionState());

  test::RunPendingTasks();

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));

  // Remove the binding for other tests to be able to set their own binding.
  // Otherwise, it it wll be bound already.
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

// Test that navigator.credentials.get() increments the feature use counter when
// one of the identity requests is a digital identity credential using the
// modern request format.
TEST_F(DigitalIdentityCredentialTest, IdentityDigitalCredentialUseCounter) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request())));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  // Modern requests, contains an Object, and hence are passed over to the
  // request as Value.
  EXPECT_CALL(*mock_request(),
              Get(AllOf(SizeIs(1), FirstRequestDataContainsValue()),
                  mojom::GetRequestFormat::kModern, _));
  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateValidGetOptions(context.GetScriptState()),
      context.GetExceptionState());

  test::RunPendingTasks();

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));
}

// Test that navigator.credentials.get() doesn't forward request with the modern
// format when the request data are formatted as a string instead of an object.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialForModernFormatAndStringRequestData) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request())));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  // malformed request that uses the modern format with the legacy string
  // request data format.
  DigitalCredentialGetRequest* request = DigitalCredentialGetRequest::Create();
  request->setProtocol("openid4vp");
  request->setData(
      MakeGarbageCollected<V8UnionObjectOrString>(String("request")));
  HeapVector<Member<DigitalCredentialGetRequest>> requests;
  requests.push_back(request);

  // The request will dropped at the rendered layer since it uses the modern
  // format while the request data is using the legacy string format.
  EXPECT_CALL(*mock_request(), Get).Times(0);
  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateGetOptionsWithRequests(requests),
      context.GetExceptionState());

  test::RunPendingTasks();

  EXPECT_FALSE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_FALSE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));

  // Remove the binding for other tests to be able to set their own binding.
  // Otherwise, it it wll be bound already.
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

// Test that navigator.credentials.get() doesn't forward request with the legacy
// format when the request data are formatted as an object instead of a string.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialForLegacyFormatAndObjectRequestData) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request())));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  // malformed request that uses the legacy format with the modern object
  // request data format.
  IdentityRequestProvider* identity_provider =
      IdentityRequestProvider::Create();
  identity_provider->setProtocol("openid4vp");
  v8::Local<v8::Object> request_data =
      v8::Object::New(script_state->GetIsolate());
  v8::Maybe<bool> maybe = request_data->Set(
      script_state->GetContext(), V8String(script_state->GetIsolate(), "key"),
      V8String(script_state->GetIsolate(), "value"));
  CHECK(maybe.IsJust() || maybe.FromJust());

  identity_provider->setRequest(MakeGarbageCollected<V8UnionObjectOrString>(
      ScriptObject(script_state->GetIsolate(), request_data)));
  HeapVector<Member<IdentityRequestProvider>> identity_providers;
  identity_providers.push_back(identity_provider);

  // The request will dropped at the rendered layer since it uses the legacy
  // format while the request data is using the modern object format.
  EXPECT_CALL(*mock_request(), Get).Times(0);
  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateLegacyGetOptionsWithProviders(identity_providers),
      context.GetExceptionState());

  test::RunPendingTasks();

  // The call is counted as a deprecated call.
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_FALSE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));

  // Remove the binding for other tests to be able to set their own binding.
  // Otherwise, it it wll be bound already.
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

// Test that navigator.credentials.get() doesn't forward request with legacy
// format when it's not supported anymore.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialForLegacyRequestFormatNotSupported) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);
  ScopedWebIdentityDigitalCredentialsStopSupportingLegacyRequestFormatForTest
      scoped_no_legacy_format_support(/*enabled=*/true);

  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request())));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  // All requests will dropped at the rendered layer since they use the legacy
  // format which isn't supported anymore, and hence the request won't be
  // invoked.
  EXPECT_CALL(*mock_request(), Get).Times(0);
  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateLegacyGetValidOptions(), context.GetExceptionState());

  test::RunPendingTasks();

  EXPECT_FALSE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_FALSE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));

  // Remove the binding for other tests to be able to set their own binding.
  // Otherwise, it it wll be bound already.
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

// Test that navigator.credentials.create() increments the feature use counter
// when one of the identity providers is a digital identity credential.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialCreateUseCounter) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsCreationForTest scoped_digital_credentials(
      /*enabled=*/true);

  std::unique_ptr mock_request = std::make_unique<MockDigitalIdentityRequest>();
  auto mock_request_ptr = mock_request.get();
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request_ptr)));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  CreateDigitalIdentityCredentialInExternalSource(
      resolver, *CreateValidCreateOptions(), context.GetExceptionState());

  test::RunPendingTasks();

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));

  // Remove the binding for other tests to be able to set their own binding.
  // Otherwise, it wll be bound already.
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

}  // namespace blink
