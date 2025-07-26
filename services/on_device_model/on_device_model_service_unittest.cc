// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include "base/files/scoped_temp_file.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/fake/fake_chrome_ml_api.h"
#include "services/on_device_model/fake/on_device_model_fake.h"
#include "services/on_device_model/ml/chrome_ml_types.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;

class ContextClientWaiter : public mojom::ContextClient {
 public:
  mojo::PendingRemote<mojom::ContextClient> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnComplete(uint32_t tokens_processed) override {
    tokens_processed_ = tokens_processed;
    run_loop_.Quit();
  }

  int WaitForCompletion() {
    run_loop_.Run();
    return tokens_processed_;
  }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::ContextClient> receiver_{this};
  int tokens_processed_ = 0;
};

class FakeFile {
 public:
  explicit FakeFile(const std::string& content) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(temp_file_.Create());
    base::File file(temp_file_.path(), base::File::FLAG_OPEN |
                                           base::File::FLAG_WRITE |
                                           base::File::FLAG_READ);
    CHECK(file.IsValid());
    file.WriteAtCurrentPos(base::as_byte_span(content));
  }
  ~FakeFile() = default;

  base::File Open() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::File(temp_file_.path(), base::File::FLAG_OPEN |
                                             base::File::FLAG_WRITE |
                                             base::File::FLAG_READ);
  }

  base::FilePath Path() { return temp_file_.path(); }

 private:
  base::ScopedTempFile temp_file_;
};

class OnDeviceModelServiceTest : public testing::Test {
 public:
  OnDeviceModelServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver(),
                      *fake_ml::GetFakeChromeML()) {}

  mojo::Remote<mojom::OnDeviceModelService>& service() { return service_; }

  mojo::Remote<mojom::OnDeviceModel> LoadModel(
      ml::ModelBackendType backend_type = ml::ModelBackendType::kGpuBackend,
      ml::ModelPerformanceHint performance_hint =
          ml::ModelPerformanceHint::kHighestQuality) {
    base::RunLoop run_loop;
    mojo::Remote<mojom::OnDeviceModel> remote;
    auto params = mojom::LoadModelParams::New();
    params->backend_type = backend_type;
    params->performance_hint = performance_hint;
    params->max_tokens = 8000;
    service()->LoadModel(
        std::move(params), remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](mojom::LoadModelResult result) {
          EXPECT_EQ(mojom::LoadModelResult::kSuccess, result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return remote;
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptationWithParams(
      mojom::OnDeviceModel& model,
      mojom::LoadAdaptationParamsPtr adaptation_params) {
    base::RunLoop run_loop;
    mojo::Remote<mojom::OnDeviceModel> remote;
    model.LoadAdaptation(
        std::move(adaptation_params), remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](mojom::LoadModelResult result) {
          EXPECT_EQ(mojom::LoadModelResult::kSuccess, result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return remote;
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptation(
      mojom::OnDeviceModel& model,
      base::File adaptation_data) {
    auto params = mojom::LoadAdaptationParams::New();
    params->assets.weights = std::move(adaptation_data);
    return LoadAdaptationWithParams(model, std::move(params));
  }

  mojo::Remote<mojom::OnDeviceModel> LoadAdaptation(
      mojom::OnDeviceModel& model,
      base::FilePath adaptation_path) {
    auto params = mojom::LoadAdaptationParams::New();
    params->assets.weights_path = std::move(adaptation_path);
    return LoadAdaptationWithParams(model, std::move(params));
  }

  mojom::AppendOptionsPtr MakeInput(const std::string& input) {
    return MakeInput({ml::InputPiece(input)});
  }

  mojom::AppendOptionsPtr MakeInput(std::vector<ml::InputPiece> input) {
    auto options = mojom::AppendOptions::New();
    options->input = mojom::Input::New(std::move(input));
    return options;
  }

  std::vector<std::string> GetResponses(mojom::OnDeviceModel& model,
                                        const std::string& input) {
    TestResponseHolder response;
    mojo::Remote<mojom::Session> session;
    model.StartSession(session.BindNewPipeAndPassReceiver());
    auto options = mojom::AppendOptions::New();
    options->input =
        mojom::Input::New(std::vector<ml::InputPiece>{ml::InputPiece(input)});
    session->Append(std::move(options), {});
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    return response.responses();
  }

  size_t GetNumModels() { return service_impl_.NumModelsForTesting(); }

  void FlushService() { service_.FlushForTesting(); }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  mojo::Remote<mojom::OnDeviceModelService> service_;
  OnDeviceModelService service_impl_;
};

TEST_F(OnDeviceModelServiceTest, Responds) {
  auto model = LoadModel();
  EXPECT_THAT(GetResponses(*model, "bar"), ElementsAre("Context: bar\n"));
  // Try another input on  the same model.
  EXPECT_THAT(GetResponses(*model, "cat"), ElementsAre("Context: cat\n"));
}

TEST_F(OnDeviceModelServiceTest, Append) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});
  session->Append(MakeInput("cheddar"), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(),
              ElementsAre("Context: cheese\n", "Context: more\n",
                          "Context: cheddar\n"));
}

TEST_F(OnDeviceModelServiceTest, CloneContextAndContinue) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});

  mojo::Remote<mojom::Session> cloned;
  session->Clone(cloned.BindNewPipeAndPassReceiver());

  {
    TestResponseHolder response;
    cloned->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(response.responses(),
                ElementsAre("Context: cheese\n", "Context: more\n"));
  }
  {
    TestResponseHolder response;
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(response.responses(),
                ElementsAre("Context: cheese\n", "Context: more\n"));
  }

  session->Append(MakeInput("foo"), {});
  cloned->Append(MakeInput("bar"), {});
  {
    TestResponseHolder response;
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(
        response.responses(),
        ElementsAre("Context: cheese\n", "Context: more\n", "Context: foo\n"));
  }
  {
    TestResponseHolder response;
    cloned->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
    EXPECT_THAT(
        response.responses(),
        ElementsAre("Context: cheese\n", "Context: more\n", "Context: bar\n"));
  }
}

TEST_F(OnDeviceModelServiceTest, MultipleSessionsAppend) {
  auto model = LoadModel();

  TestResponseHolder response1, response2, response3, response4, response5;
  mojo::Remote<mojom::Session> session1, session2, session3, session4, session5;

  model->StartSession(session1.BindNewPipeAndPassReceiver());
  model->StartSession(session2.BindNewPipeAndPassReceiver());

  session1->Append(MakeInput("cheese"), {});
  session1->Append(MakeInput("more"), {});
  session2->Append(MakeInput("apple"), {});

  session1->Clone(session3.BindNewPipeAndPassReceiver());
  session1->Append(MakeInput("cheddar"), {});
  session1->Generate(mojom::GenerateOptions::New(), response1.BindRemote());

  session2->Append(MakeInput("banana"), {});

  session2->Clone(session4.BindNewPipeAndPassReceiver());
  session2->Append(MakeInput("candy"), {});
  session2->Generate(mojom::GenerateOptions::New(), response2.BindRemote());

  session4->Clone(session5.BindNewPipeAndPassReceiver());
  session4->Append(MakeInput("chip"), {});
  session4->Generate(mojom::GenerateOptions::New(), response3.BindRemote());

  session3->Append(MakeInput("choco"), {});
  session3->Generate(mojom::GenerateOptions::New(), response4.BindRemote());

  session5->Append(MakeInput("orange"), {});
  session5->Generate(mojom::GenerateOptions::New(), response5.BindRemote());

  response1.WaitForCompletion();
  response2.WaitForCompletion();
  response3.WaitForCompletion();
  response4.WaitForCompletion();
  response5.WaitForCompletion();

  EXPECT_THAT(response1.responses(),
              ElementsAre("Context: cheese\n", "Context: more\n",
                          "Context: cheddar\n"));
  EXPECT_THAT(
      response2.responses(),
      ElementsAre("Context: apple\n", "Context: banana\n", "Context: candy\n"));
  EXPECT_THAT(
      response3.responses(),
      ElementsAre("Context: apple\n", "Context: banana\n", "Context: chip\n"));
  EXPECT_THAT(
      response4.responses(),
      ElementsAre("Context: cheese\n", "Context: more\n", "Context: choco\n"));
  EXPECT_THAT(response5.responses(),
              ElementsAre("Context: apple\n", "Context: banana\n",
                          "Context: orange\n"));
}

TEST_F(OnDeviceModelServiceTest, CountTokens) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->Append(MakeInput("cheese"), {});
  session->Append(MakeInput("more"), {});

  std::string input = "cheddar";
  session->Append(MakeInput(input), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  // 3 context.
  EXPECT_THAT(response.output_token_count(), 3);
}

TEST_F(OnDeviceModelServiceTest, AppendWithTokenLimits) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());

  std::string input = "big cheese";
  ContextClientWaiter client1;
  auto max_input = MakeInput("big cheese");
  max_input->max_tokens = 4;
  session->Append(std::move(max_input), client1.BindRemote());
  EXPECT_EQ(client1.WaitForCompletion(), 4);

  ContextClientWaiter client2;
  auto offset_input = MakeInput("big cheese");
  offset_input->token_offset = 4;
  session->Append(std::move(offset_input), client2.BindRemote());
  EXPECT_EQ(client2.WaitForCompletion(), 6);

  session->Append(MakeInput("cheddar"), {});
  session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(),
              ElementsAre("Context: big \n", "Context: cheese\n",
                          "Context: cheddar\n"));
}

TEST_F(OnDeviceModelServiceTest, MultipleSessionsWaitPreviousSession) {
  auto model = LoadModel();

  TestResponseHolder response1;
  mojo::Remote<mojom::Session> session1;
  model->StartSession(session1.BindNewPipeAndPassReceiver());
  session1->Append(MakeInput("1"), {});
  session1->Generate(mojom::GenerateOptions::New(), response1.BindRemote());

  mojo::Remote<mojom::Session> session2;
  model->StartSession(session2.BindNewPipeAndPassReceiver());

  // First session should not get canceled.
  session1.reset_on_disconnect();
  FlushService();
  EXPECT_TRUE(session1);

  // Response from first session should still work.
  response1.WaitForCompletion();
  EXPECT_THAT(response1.responses(), ElementsAre("Context: 1\n"));

  // Second session still works.
  TestResponseHolder response2;
  session2->Append(MakeInput("2"), {});
  session2->Generate(mojom::GenerateOptions::New(), response2.BindRemote());
  response2.WaitForCompletion();
  EXPECT_THAT(response2.responses(), ElementsAre("Context: 2\n"));
}

TEST_F(OnDeviceModelServiceTest, LoadsAdaptation) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  auto model = LoadModel();
  auto adaptation1 = LoadAdaptation(*model, weights1.Open());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("Context: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1\n", "Context: foo\n"));

  auto adaptation2 = LoadAdaptation(*model, weights2.Open());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("Context: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1\n", "Context: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation2, "foo"),
              ElementsAre("Adaptation: Adapt2\n", "Context: foo\n"));
}

TEST_F(OnDeviceModelServiceTest, DestroysAdaptationSession) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  auto model = LoadModel();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_ml::GetActiveNonCloneSessions(), 1);

  auto adaptation1 = LoadAdaptation(*model, weights1.Open());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_ml::GetActiveNonCloneSessions(), 2);

  auto adaptation2 = LoadAdaptation(*model, weights2.Open());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_ml::GetActiveNonCloneSessions(), 3);

  adaptation1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_ml::GetActiveNonCloneSessions(), 2);

  adaptation2.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_ml::GetActiveNonCloneSessions(), 1);
}

TEST_F(OnDeviceModelServiceTest, LoadsAdaptationWithPath) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  auto model = LoadModel(ml::ModelBackendType::kApuBackend);
  auto adaptation1 = LoadAdaptation(*model, weights1.Path());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("Context: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1\n", "Context: foo\n"));

  auto adaptation2 = LoadAdaptation(*model, weights2.Path());
  EXPECT_THAT(GetResponses(*model, "foo"), ElementsAre("Context: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation1, "foo"),
              ElementsAre("Adaptation: Adapt1\n", "Context: foo\n"));
  EXPECT_THAT(GetResponses(*adaptation2, "foo"),
              ElementsAre("Adaptation: Adapt2\n", "Context: foo\n"));
}

TEST_F(OnDeviceModelServiceTest, LoadingAdaptationDoesNotCancelSession) {
  FakeFile weights1("Adapt1");
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session.reset_on_disconnect();

  LoadAdaptation(*model, weights1.Open());
  FlushService();
  EXPECT_TRUE(session);
}

TEST_F(OnDeviceModelServiceTest, DeletesModel) {
  FakeFile weights1("Adapt1");
  FakeFile weights2("Adapt2");
  FakeFile weights3("Adapt3");
  auto model1 = LoadModel();
  auto adaptation1 = LoadAdaptation(*model1, weights1.Open());
  auto adaptation2 = LoadAdaptation(*model1, weights2.Open());
  EXPECT_EQ(GetNumModels(), 1u);

  auto model2 = LoadModel();
  auto adaptation3 = LoadAdaptation(*model2, weights3.Open());
  EXPECT_EQ(GetNumModels(), 2u);

  adaptation1.reset();
  adaptation2.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 2u);

  model1.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 1u);

  model2.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 1u);

  adaptation3.reset();
  FlushService();
  EXPECT_EQ(GetNumModels(), 0u);
}

TEST_F(OnDeviceModelServiceTest, Score) {
  auto model = LoadModel();

  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  session->Append(MakeInput("hi"), {});

  {
    base::test::TestFuture<float> future;
    session->Score("x", future.GetCallback());
    EXPECT_EQ(future.Get(), float('x'));
  }
  {
    base::test::TestFuture<float> future;
    session->Score("y", future.GetCallback());
    EXPECT_EQ(future.Get(), float('y'));
  }
}

TEST_F(OnDeviceModelServiceTest, AppendWithTokens) {
  auto model = LoadModel();

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  model->StartSession(session.BindNewPipeAndPassReceiver());
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kSystem);
    pieces.push_back("hi");
    pieces.push_back(ml::Token::kEnd);
    session->Append(MakeInput(std::move(pieces)), {});
  }
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kModel);
    pieces.push_back("hello");
    pieces.push_back(ml::Token::kEnd);
    session->Append(MakeInput(std::move(pieces)), {});
  }
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kUser);
    pieces.push_back("bye");
    session->Append(MakeInput(std::move(pieces)), {});
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
  }
  response.WaitForCompletion();

  EXPECT_THAT(response.responses(), ElementsAre("Context: System: hi End.\n",
                                                "Context: Model: hello End.\n",
                                                "Context: User: bye\n"));
}

TEST_F(OnDeviceModelServiceTest, AppendWithImages) {
  auto model = LoadModel();
  auto params = mojom::LoadAdaptationParams::New();
  params->enable_image_input = true;
  auto adaptation = LoadAdaptationWithParams(*model, std::move(params));

  TestResponseHolder response;
  mojo::Remote<mojom::Session> session;
  adaptation->StartSession(session.BindNewPipeAndPassReceiver());

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("cheddar");

    SkBitmap cheesy_bitmap;
    cheesy_bitmap.allocPixels(
        SkImageInfo::Make(7, 21, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
        0);
    cheesy_bitmap.eraseColor(SK_ColorYELLOW);
    pieces.push_back(cheesy_bitmap);

    pieces.push_back("cheese");

    session->Append(MakeInput(std::move(pieces)), {});
  }

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("bleu");

    SkBitmap moldy_cheese;
    moldy_cheese.allocPixels(
        SkImageInfo::Make(63, 42, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
        0);
    moldy_cheese.eraseColor(SK_ColorBLUE);
    pieces.push_back(moldy_cheese);

    pieces.push_back("cheese");

    session->Append(MakeInput(std::move(pieces)), {});
    session->Generate(mojom::GenerateOptions::New(), response.BindRemote());
    response.WaitForCompletion();
  }

  EXPECT_THAT(response.responses(),
              ElementsAre("Context: cheddar[Bitmap of size 7x21]cheese\n",
                          "Context: bleu[Bitmap of size 63x42]cheese\n"));
}

TEST_F(OnDeviceModelServiceTest, ClassifyTextSafety) {
  FakeFile ts_data("fake_ts_data");
  FakeFile ts_sp_model("fake_ts_sp_model");
  TextSafetyLoaderParams params;
  params.ts_paths.emplace();
  params.ts_paths->data = ts_data.Path();
  params.ts_paths->sp_model = ts_sp_model.Path();
  mojo::Remote<mojom::TextSafetyModel> model;
  service()->LoadTextSafetyModel(LoadTextSafetyParams(params),
                                 model.BindNewPipeAndPassReceiver());
  base::test::TestFuture<mojom::SafetyInfoPtr> future1;
  base::test::TestFuture<mojom::SafetyInfoPtr> future2;
  model->ClassifyTextSafety("unsafe text", future1.GetCallback());
  model->ClassifyTextSafety("reasonable text", future2.GetCallback());
  auto resp1 = future1.Take();
  auto resp2 = future2.Take();

  ASSERT_TRUE(resp1);
  EXPECT_THAT(resp1->class_scores, ElementsAre(0.8, 0.8));
  ASSERT_TRUE(resp2);
  EXPECT_THAT(resp2->class_scores, ElementsAre(0.2, 0.2));
}

TEST_F(OnDeviceModelServiceTest, PerformanceHint) {
  auto model = LoadModel(ml::ModelBackendType::kGpuBackend,
                         ml::ModelPerformanceHint::kFastestInference);
  EXPECT_THAT(GetResponses(*model, "foo"),
              ElementsAre("Fastest inference\n", "Context: foo\n"));
}

}  // namespace
}  // namespace on_device_model
