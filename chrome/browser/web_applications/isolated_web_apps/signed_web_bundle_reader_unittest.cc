// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::Eq;
using testing::IsTrue;
using testing::Message;
using testing::Property;
using testing::UnorderedElementsAre;

constexpr std::array<uint8_t, 32> kEd25519PublicKey = {
    0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2,
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0};

class FakeSignatureVerifier
    : public web_package::SignedWebBundleSignatureVerifier {
 public:
  explicit FakeSignatureVerifier(
      absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
          error)
      : error_(error) {}

  void VerifySignatures(
      base::File file,
      web_package::SignedWebBundleIntegrityBlock integrity_block,
      SignatureVerificationCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error_));
  }

 private:
  absl::optional<web_package::SignedWebBundleSignatureVerifier::Error> error_;
};

}  // namespace

class SignedWebBundleReaderWithRealBundlesTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    SetTrustedWebBundleIdsForTesting(
        {*web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId)});
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  using ErrorForTesting = web_package::WebBundleSigner::ErrorForTesting;
  using VerificationAction = SignedWebBundleReader::SignatureVerificationAction;

  std::unique_ptr<SignedWebBundleReader> CreateReaderAndInitialize(
      const TestSignedWebBundleBuilder::BuildOptions& build_options,
      SignedWebBundleReader::ReadErrorCallback callback,
      VerificationAction verification_action =
          VerificationAction::ContinueAndVerifySignatures(),
      absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
          signature_verifier_error = absl::nullopt,
      const std::string test_file_data = kHtmlString) {
    base::FilePath swbn_file_path =
        temp_dir_.GetPath().Append(base::FilePath::FromASCII("bundle.swbn"));
    TestSignedWebBundle bundle =
        TestSignedWebBundleBuilder::BuildDefault(build_options);
    EXPECT_THAT(base::WriteFile(swbn_file_path, bundle.data), IsTrue());

    const GURL base_url = build_options.base_url_.has_value()
                              ? build_options.base_url_.value()
                              : kUrl;
    std::unique_ptr<SignedWebBundleReader> reader =
        SignedWebBundleReader::Create(
            swbn_file_path, base_url,
            std::make_unique<FakeSignatureVerifier>(signature_verifier_error));

    reader->StartReading(
        base::BindLambdaForTesting(
            [verification_action](
                web_package::SignedWebBundleIntegrityBlock integrity_block,
                base::OnceCallback<void(VerificationAction)>
                    verification_action_callback) {
              EXPECT_THAT(integrity_block.signature_stack().size(), Eq(1ul));
              EXPECT_THAT(integrity_block.signature_stack()
                              .entries()[0]
                              .public_key()
                              .bytes(),
                          testing::ElementsAreArray(kTestPublicKey));

              std::move(verification_action_callback).Run(verification_action);
            }),
        std::move(callback));

    return reader;
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
  const GURL kUrl = GURL("https://example.com");
  constexpr static char kHtmlString[] = "test";
};

// Note that Isolated Web Apps (IWAs) don't support having primary URLs, but the
// reader does, as it can be used for any Signed Web Bundle, even those not
// compatible with IWAs. Also, when baseURL is empty, relative URLs are used.
TEST_F(SignedWebBundleReaderWithRealBundlesTest,
       ReadValidWebBundleWithPrimaryUrlAndRelativeUrls) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;

  auto reader = CreateReaderAndInitialize(
      TestSignedWebBundleBuilder::BuildOptions().SetPrimaryUrl(kUrl),
      parse_status_future.GetCallback());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);
  EXPECT_TRUE(reader->GetPrimaryURL().has_value());
  EXPECT_EQ(reader->GetEntries().size(), 2ul);
  EXPECT_THAT(reader->GetEntries(),
              UnorderedElementsAre(
                  kUrl.Resolve(TestSignedWebBundleBuilder::kTestIconUrl),
                  kUrl.Resolve(TestSignedWebBundleBuilder::kTestManifestUrl)));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest, ReadValidResponse) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;

  auto reader =
      CreateReaderAndInitialize(TestSignedWebBundleBuilder::BuildOptions()
                                    .SetBaseUrl(kUrl)
                                    .SetIndexHTMLContent(kHtmlString),
                                parse_status_future.GetCallback());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);
  EXPECT_FALSE(reader->GetPrimaryURL().has_value());
  EXPECT_EQ(reader->GetEntries().size(), 3ul);
  EXPECT_THAT(reader->GetEntries(),
              UnorderedElementsAre(
                  kUrl.Resolve(TestSignedWebBundleBuilder::kTestHtmlUrl),
                  kUrl.Resolve(TestSignedWebBundleBuilder::kTestIconUrl),
                  kUrl.Resolve(TestSignedWebBundleBuilder::kTestManifestUrl)));

  network::ResourceRequest resource_request;
  resource_request.url = kUrl.Resolve(TestSignedWebBundleBuilder::kTestHtmlUrl);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  ASSERT_OK_AND_ASSIGN(auto response, response_result.Take());
  EXPECT_EQ(response->payload_length, std::string(kHtmlString).length());
  EXPECT_EQ(response->response_code, 200);
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest,
       ReadIntegrityBlockWithInvalidVersion) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;

  auto reader = CreateReaderAndInitialize(
      TestSignedWebBundleBuilder::BuildOptions()
          .SetBaseUrl(kUrl)
          .SetIndexHTMLContent(kHtmlString)
          .SetErrorsForTesting({ErrorForTesting::kInvalidVersion}),
      parse_status_future.GetCallback());

  auto parse_status = parse_status_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserVersionError)));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest,
       ReadIntegrityBlockWithInvalidStructure) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;

  auto reader = CreateReaderAndInitialize(
      TestSignedWebBundleBuilder::BuildOptions()
          .SetBaseUrl(kUrl)
          .SetErrorsForTesting(
              {ErrorForTesting::kInvalidIntegrityBlockStructure}),
      parse_status_future.GetCallback());

  auto parse_status = parse_status_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError)));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest, ReadIntegrityBlockAndAbort) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;

  auto reader = CreateReaderAndInitialize(
      TestSignedWebBundleBuilder::BuildOptions().SetBaseUrl(kUrl),
      parse_status_future.GetCallback(),
      VerificationAction::Abort("test error"));

  auto parse_status = parse_status_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(
      parse_status,
      ErrorIs(AllOf(
          Property(
              &UnusableSwbnFileError::value,
              UnusableSwbnFileError::Error::kIntegrityBlockValidationError),
          Property(&UnusableSwbnFileError::message, "test error"))));
}

TEST_F(SignedWebBundleReaderWithRealBundlesTest, Close) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;

  auto reader =
      CreateReaderAndInitialize(TestSignedWebBundleBuilder::BuildOptions()
                                    .SetBaseUrl(kUrl)
                                    .SetIndexHTMLContent(kHtmlString),
                                parse_status_future.GetCallback());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  base::test::TestFuture<void> close_future;
  reader->Close(close_future.GetCallback());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kClosed);
}

class SignedWebBundleReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    parser_factory_ =
        std::make_unique<web_package::MockWebBundleParserFactory>();

    response_ = web_package::mojom::BundleResponse::New();
    response_->response_code = 200;
    response_->payload_offset = 0;
    response_->payload_length = sizeof(kResponseBody) - 1;

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> items;
    items.insert(
        {kUrl, web_package::mojom::BundleResponseLocation::New(
                   response_->payload_offset, response_->payload_length)});
    metadata_ = web_package::mojom::BundleMetadata::New();
    metadata_->primary_url = kUrl;
    metadata_->requests = std::move(items);

    web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        signature_stack_entry =
            web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
    signature_stack_entry->public_key = web_package::Ed25519PublicKey::Create(
        base::make_span(kEd25519PublicKey));
    signature_stack_entry->signature = web_package::Ed25519Signature::Create(
        base::make_span(kEd25519Signature));

    std::vector<web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        signature_stack;
    signature_stack.push_back(std::move(signature_stack_entry));

    integrity_block_ = web_package::mojom::BundleIntegrityBlock::New();
    integrity_block_->size = 123;
    integrity_block_->signature_stack = std::move(signature_stack);
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  using VerificationAction = SignedWebBundleReader::SignatureVerificationAction;

  std::unique_ptr<SignedWebBundleReader> CreateReaderAndInitialize(
      SignedWebBundleReader::ReadErrorCallback callback,
      VerificationAction verification_action =
          VerificationAction::ContinueAndVerifySignatures(),
      absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
          signature_verifier_error = absl::nullopt,
      const absl::optional<GURL>& base_url = absl::nullopt,
      const std::string test_file_data = kResponseBody) {
    // Provide a buffer that contains the contents of just a single
    // response. We do not need to provide an integrity block or metadata
    // here, since reading them is completely mocked. Only response bodies
    // are read from an actual (temporary) file.
    base::FilePath temp_file_path;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path));
    EXPECT_TRUE(base::WriteFile(temp_file_path, test_file_data));

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));

    std::unique_ptr<SignedWebBundleReader> reader =
        SignedWebBundleReader::Create(
            temp_file_path, base_url,
            std::make_unique<FakeSignatureVerifier>(signature_verifier_error));

    reader->StartReading(
        base::BindLambdaForTesting(
            [verification_action](
                web_package::SignedWebBundleIntegrityBlock integrity_block,
                base::OnceCallback<void(VerificationAction)> callback) {
              EXPECT_THAT(integrity_block.signature_stack().size(), Eq(1ul));
              EXPECT_THAT(integrity_block.signature_stack()
                              .entries()[0]
                              .public_key()
                              .bytes(),
                          Eq(kEd25519PublicKey));

              std::move(callback).Run(verification_action);
            }),
        std::move(callback));

    return reader;
  }

  base::expected<web_package::mojom::BundleResponsePtr,
                 SignedWebBundleReader::ReadResponseError>
  ReadAndFulfillResponse(
      SignedWebBundleReader& reader,
      const network::ResourceRequest& resource_request,
      web_package::mojom::BundleResponseLocationPtr expected_read_response_args,
      web_package::mojom::BundleResponsePtr response,
      web_package::mojom::BundleResponseParseErrorPtr error = nullptr) {
    base::test::TestFuture<
        base::expected<web_package::mojom::BundleResponsePtr,
                       SignedWebBundleReader::ReadResponseError>>
        response_result;
    reader.ReadResponse(resource_request, response_result.GetCallback());

    parser_factory_->RunResponseCallback(std::move(expected_read_response_args),
                                         std::move(response), std::move(error));

    return response_result.Take();
  }

  void SimulateAndWaitForParserDisconnect(SignedWebBundleReader& reader) {
    base::RunLoop run_loop;
    reader.SetParserDisconnectCallbackForTesting(run_loop.QuitClosure());
    parser_factory_->SimulateParserDisconnect();
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;

  const GURL kUrl = GURL("https://example.com");
  web_package::mojom::BundleMetadataPtr metadata_;

  constexpr static char kResponseBody[] = "test";
  web_package::mojom::BundleResponsePtr response_;
};

TEST_F(SignedWebBundleReaderTest, ReadValidIntegrityBlockAndMetadata) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  base::HistogramTester histogram_tester;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  EXPECT_EQ(reader->GetPrimaryURL(), kUrl);
  EXPECT_EQ(reader->GetEntries().size(), 1ul);
  EXPECT_EQ(reader->GetEntries()[0], kUrl);

  histogram_tester.ExpectTotalCount(
      "WebApp.Isolated.SignatureVerificationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "WebApp.Isolated.SignatureVerificationFileLength", 1);
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockError) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(
      nullptr, web_package::mojom::BundleIntegrityBlockParseError::New());

  auto parse_status = parse_status_future.Take();

  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockWithParserCrash) {
  parser_factory_->SimulateParseIntegrityBlockCrash();
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  auto parse_status = parse_status_future.Take();

  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(
      parse_status,
      ErrorIs(Property(
          &UnusableSwbnFileError::value,
          UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError)));
}

class SignedWebBundleReaderSignatureVerificationErrorTest
    : public SignedWebBundleReaderTest,
      public ::testing::WithParamInterface<
          web_package::SignedWebBundleSignatureVerifier::Error> {};

TEST_P(SignedWebBundleReaderSignatureVerificationErrorTest,
       SignatureVerificationError) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(
      parse_status_future.GetCallback(),
      VerificationAction::ContinueAndVerifySignatures(), GetParam());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);

  auto expected_status = UnusableSwbnFileError(GetParam());

  EXPECT_EQ(parse_status.error(), expected_status);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignedWebBundleReaderSignatureVerificationErrorTest,
    ::testing::Values(
        web_package::SignedWebBundleSignatureVerifier::Error::ForInternalError(
            "internal error"),
        web_package::SignedWebBundleSignatureVerifier::Error::
            ForInvalidSignature("invalid signature")));

#if BUILDFLAG(IS_CHROMEOS)

// Test that signatures are not verified when the
// `integrity_block_callback` asks to skip signature verification and
// thus the provided `web_package::SignedWebBundleSignatureVerifier::Error` is
// never triggered.
TEST_F(SignedWebBundleReaderTest,
       ReadIntegrityBlockAndSkipSignatureVerification) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(
      parse_status_future.GetCallback(),
      VerificationAction::ContinueAndSkipSignatureVerification(),
      web_package::SignedWebBundleSignatureVerifier::Error::ForInvalidSignature(
          "invalid signature"));

  parser_factory_->RunIntegrityBlockCallback(integrity_block_.Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(SignedWebBundleReaderTest, ReadMetadataError) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(
      integrity_block_->size, nullptr,
      web_package::mojom::BundleMetadataParseError::New());

  auto parse_status = parse_status_future.Take();

  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(parse_status,
              ErrorIs(Property(
                  &UnusableSwbnFileError::value,
                  UnusableSwbnFileError::Error::kMetadataParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadMetadataWithParserCrash) {
  parser_factory_->SimulateParseMetadataCrash();
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_status = parse_status_future.Take();

  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_THAT(parse_status,
              ErrorIs(Property(
                  &UnusableSwbnFileError::value,
                  UnusableSwbnFileError::Error::kMetadataParserInternalError)));
}

TEST_F(SignedWebBundleReaderTest, ReadResponse) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->payload_offset, response_->payload_offset);
  EXPECT_EQ(response->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithFragment) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetRefStr("baz");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->payload_offset, response_->payload_offset);
  EXPECT_EQ(response->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithPath) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetPathStr("/foo");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(response.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "https://example.com/foo");
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithQuery) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetQueryStr("foo");
  resource_request.url = kUrl.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(response.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "https://example.com/?foo");
}

TEST_F(SignedWebBundleReaderTest, ReadResponseError) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request, metadata_->requests[kUrl]->Clone(),
      nullptr,
      web_package::mojom::BundleResponseParseError::New(
          web_package::mojom::BundleParseErrorType::kFormatError, "test"));
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kFormatError);
  EXPECT_EQ(response.error().message, "test");
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserDisconnect) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  SimulateAndWaitForParserDisconnect(*reader.get());
  {
    ASSERT_OK_AND_ASSIGN(auto response, ReadAndFulfillResponse(
                                            *reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
    EXPECT_EQ(response->response_code, 200);
    EXPECT_EQ(response->payload_offset, response_->payload_offset);
    EXPECT_EQ(response->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 2);

  // Simulate another disconnect to verify that the reader can recover from
  // multiple disconnects over the course of its lifetime.
  SimulateAndWaitForParserDisconnect(*reader.get());
  {
    ASSERT_OK_AND_ASSIGN(auto response, ReadAndFulfillResponse(
                                            *reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));
    EXPECT_EQ(response->response_code, 200);
    EXPECT_EQ(response->payload_offset, response_->payload_offset);
    EXPECT_EQ(response->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 3);
}

TEST_F(SignedWebBundleReaderTest,
       SimulateParserDisconnectWithFileErrorWhenReconnecting) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  SimulateAndWaitForParserDisconnect(*reader.get());
  reader->SetReconnectionFileErrorForTesting(
      base::File::Error::FILE_ERROR_ACCESS_DENIED);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());
  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(
      response.error().type,
      SignedWebBundleReader::ReadResponseError::Type::kParserInternalError);
  EXPECT_EQ(response.error().message,
            "Unable to open file: FILE_ERROR_ACCESS_DENIED");
  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 1);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserCrash) {
  parser_factory_->SimulateParseResponseCrash();
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(
      response.error().type,
      SignedWebBundleReader::ReadResponseError::Type::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseBody) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));

  std::string response_body =
      ReadAndFulfillResponseBody(*reader.get(), std::move(response));
  EXPECT_EQ(response_body, kResponseBody);
}

TEST_F(SignedWebBundleReaderTest, CloseWhileReadingResponseBody) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = kUrl;

  ASSERT_OK_AND_ASSIGN(
      auto response, ReadAndFulfillResponse(*reader.get(), resource_request,
                                            metadata_->requests[kUrl]->Clone(),
                                            response_->Clone()));

  const uint64_t response_body_length = response->payload_length;
  auto read_response_body_callable =
      base::BindOnce(&SignedWebBundleReader::ReadResponseBody,
                     base::Unretained(reader.get()), std::move(response));

  base::test::TestFuture<net::Error> on_response_read_callback;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer = ReadResponseBody(
      response_body_length, std::move(read_response_body_callable),
      on_response_read_callback.GetCallback());

  base::test::TestFuture<void> close_future;
  reader->Close(close_future.GetCallback());

  EXPECT_EQ(net::OK, on_response_read_callback.Get());
  std::vector<char> buffer(response_body_length);
  uint32_t bytes_read = buffer.size();
  MojoResult read_result = response_body_consumer->ReadData(
      buffer.data(), &bytes_read, MOJO_READ_DATA_FLAG_NONE);
  EXPECT_EQ(MOJO_RESULT_OK, read_result);
  EXPECT_EQ(buffer.size(), bytes_read);
  EXPECT_EQ(std::string(buffer.data(), bytes_read), kResponseBody);

  ASSERT_TRUE(close_future.Wait());
}

TEST_F(SignedWebBundleReaderTest, ResponseBodyEndDoesntFitInUint64) {
  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(parse_status_future.GetCallback());
  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());
  ASSERT_TRUE(parse_status_future.Wait());

  auto response = web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  // End of the response (which is offset + length) should not fit into
  // unit64_t.
  response->payload_offset = std::numeric_limits<uint64_t>::max() - 10;
  response->payload_length = 11;
  const uint64_t response_body_length = response->payload_length;

  auto read_response_body_callable =
      base::BindOnce(&SignedWebBundleReader::ReadResponseBody,
                     base::Unretained(reader.get()), std::move(response));

  base::test::TestFuture<net::Error> on_response_read_callback;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer = ReadResponseBody(
      response_body_length, std::move(read_response_body_callable),
      on_response_read_callback.GetCallback());

  EXPECT_NE(net::OK, on_response_read_callback.Get());
}

class SignedWebBundleReaderBaseUrlTest
    : public SignedWebBundleReaderTest,
      public ::testing::WithParamInterface<absl::optional<std::string>> {
 public:
  SignedWebBundleReaderBaseUrlTest() {
    if (GetParam().has_value()) {
      base_url_ = GURL(*GetParam());
    }
  }

 protected:
  absl::optional<GURL> base_url_;
};

TEST_P(SignedWebBundleReaderBaseUrlTest, IsPassedThroughCorrectly) {
  base::test::RepeatingTestFuture<absl::optional<GURL>> on_create_parser_future;
  parser_factory_ = std::make_unique<web_package::MockWebBundleParserFactory>(
      on_create_parser_future.GetCallback());

  base::test::TestFuture<base::expected<void, UnusableSwbnFileError>>
      parse_status_future;
  auto reader = CreateReaderAndInitialize(
      parse_status_future.GetCallback(),
      VerificationAction::ContinueAndVerifySignatures(), absl::nullopt,
      base_url_);

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());
  auto parse_status = parse_status_future.Take();
  EXPECT_THAT(parse_status, HasValue());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  EXPECT_EQ(on_create_parser_future.Take(), base_url_);
  EXPECT_TRUE(on_create_parser_future.IsEmpty());

  SimulateAndWaitForParserDisconnect(*reader.get());
  network::ResourceRequest resource_request;
  resource_request.url = kUrl;
  auto response = ReadAndFulfillResponse(*reader.get(), resource_request,
                                         metadata_->requests[kUrl]->Clone(),
                                         response_->Clone());

  EXPECT_EQ(on_create_parser_future.Take(), base_url_);
  EXPECT_TRUE(on_create_parser_future.IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignedWebBundleReaderBaseUrlTest,
                         ::testing::Values(absl::nullopt,
                                           "https://example.com"));

class UnsecureSignedWebBundleReaderTest : public testing::Test {
 protected:
  using ErrorForTesting = web_package::WebBundleSigner::ErrorForTesting;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    SetTrustedWebBundleIdsForTesting(
        {*web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId)});
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(UnsecureSignedWebBundleReaderTest, ReadValidId) {
  base::FilePath path =
      temp_dir_.GetPath().Append(base::FilePath::FromASCII("test-0.swbn"));
  TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault();

  ASSERT_THAT(base::WriteFile(path, bundle.data), IsTrue());

  base::test::TestFuture<
      base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>>
      read_web_bundle_id_future;

  UnsecureSignedWebBundleIdReader::GetWebBundleId(
      path, read_web_bundle_id_future.GetCallback());
  base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>
      bundle_id_result = read_web_bundle_id_future.Take();

  ASSERT_TRUE(bundle_id_result.has_value());
  web_package::Ed25519PublicKey public_key =
      web_package::Ed25519PublicKey::Create(base::make_span(kTestPublicKey));
  EXPECT_THAT(
      bundle_id_result.value(),
      web_package::SignedWebBundleId::CreateForEd25519PublicKey(public_key));
}

TEST_F(UnsecureSignedWebBundleReaderTest, ErrorId) {
  for (auto error : {ErrorForTesting::kInvalidSignatureLength,
                     ErrorForTesting::kInvalidPublicKeyLength,
                     ErrorForTesting::kWrongSignatureStackEntryAttributeName,
                     ErrorForTesting::kNoPublicKeySignatureStackEntryAttribute,
                     ErrorForTesting::kAdditionalSignatureStackEntryAttribute,
                     ErrorForTesting::kAdditionalSignatureStackEntryElement}) {
    std::string swbn_file_name =
        base::NumberToString(base::to_underlying(error)) + "_test.swbn";
    SCOPED_TRACE(Message() << "Running testcase: "
                           << " " << swbn_file_name);

    TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions().SetErrorsForTesting(
            {error}));
    base::FilePath path =
        temp_dir_.GetPath().Append(base::FilePath::FromASCII(swbn_file_name));
    ASSERT_THAT(base::WriteFile(path, bundle.data), IsTrue());

    base::test::TestFuture<
        base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>>
        read_web_bundle_id_future;

    UnsecureSignedWebBundleIdReader::GetWebBundleId(
        path, read_web_bundle_id_future.GetCallback());
    base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>
        bundle_id_result = read_web_bundle_id_future.Take();

    ASSERT_FALSE(bundle_id_result.has_value());
    EXPECT_THAT(
        bundle_id_result,
        ErrorIs(Property(
            &UnusableSwbnFileError::value,
            UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError)));
  }
}

}  // namespace web_app
