// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/passage_embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {
namespace {

constexpr uint32_t kInputWindowSize = 256;
constexpr size_t kEmbeddingsOutputSize = 768;

class PassageEmbeddingsServiceTest : public testing::Test {
 public:
  PassageEmbeddingsServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver()) {}

  mojo::Remote<mojom::PassageEmbeddingsService>& service() { return service_; }

  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("services")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("passage_embeddings");
    embeddings_path_ =
        test_data_dir.AppendASCII("dummy_embeddings_model.tflite");
    sp_path_ = test_data_dir.AppendASCII("sentencepiece.model");
  }

  mojom::PassageEmbeddingsLoadModelsParamsPtr MakeModelParams(
      base::FilePath embeddings_path,
      base::FilePath sp_path,
      uint32_t input_window_size) {
    auto params = mojom::PassageEmbeddingsLoadModelsParams::New();
    params->embeddings_model = base::File(
        embeddings_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    params->sp_model =
        base::File(sp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    params->input_window_size = input_window_size;
    return params;
  }

  mojom::PassageEmbedderParamsPtr MakeEmbedderParams() {
    auto params = mojom::PassageEmbedderParams::New();
    params->user_initiated_priority_num_threads = 4;
    params->passive_priority_num_threads = 1;
    params->embedder_cache_size = 1000;
    return params;
  }

 protected:
  base::FilePath embeddings_path_;
  base::FilePath sp_path_;

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::PassageEmbeddingsService> service_;
  PassageEmbeddingsService service_impl_;
};

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidInputWindowSize) {
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> future;
  service()->LoadModels(
      MakeModelParams(embeddings_path_, sp_path_, 0u), MakeEmbedderParams(),
      embedder_remote.BindNewPipeAndPassReceiver(), future.GetCallback());
  bool load_models_success = future.Get();
  EXPECT_FALSE(load_models_success);
}

TEST_F(PassageEmbeddingsServiceTest, RespondsWithEmbeddings) {
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> load_models_future;
  service()->LoadModels(
      MakeModelParams(embeddings_path_, sp_path_, kInputWindowSize),
      MakeEmbedderParams(), embedder_remote.BindNewPipeAndPassReceiver(),
      load_models_future.GetCallback());
  EXPECT_TRUE(load_models_future.Get());

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future;
  embedder_remote->GenerateEmbeddings({"hello"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future.GetCallback());
  auto results = execute_future.Take();
  EXPECT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0]->embeddings.size(), kEmbeddingsOutputSize);
}

TEST_F(PassageEmbeddingsServiceTest, HistoryEmbeddingsUsesCache) {
  base::HistogramTester histogram_tester;

  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> load_models_future;
  auto model_params =
      MakeModelParams(embeddings_path_, sp_path_, kInputWindowSize);
  model_params->execute_for_gemma = false;
  service()->LoadModels(std::move(model_params), MakeEmbedderParams(),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        load_models_future.GetCallback());
  EXPECT_TRUE(load_models_future.Get());

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future1;
  embedder_remote->GenerateEmbeddings({"hello"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future1.GetCallback());
  EXPECT_EQ(execute_future1.Take().size(), 1u);
  histogram_tester.ExpectUniqueSample("History.Embeddings.Embedder.CacheHit",
                                      false, 1);

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future2;
  embedder_remote->GenerateEmbeddings({"hello"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future2.GetCallback());
  EXPECT_EQ(execute_future2.Take().size(), 1u);
  histogram_tester.ExpectBucketCount("History.Embeddings.Embedder.CacheHit",
                                     true, 1);
  histogram_tester.ExpectTotalCount("History.Embeddings.Embedder.CacheHit", 2);
}

TEST_F(PassageEmbeddingsServiceTest, WebApiBypassesCache) {
  base::HistogramTester histogram_tester;

  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> load_models_future;
  auto model_params =
      MakeModelParams(embeddings_path_, sp_path_, kInputWindowSize);
  model_params->execute_for_gemma = true;
  auto embedder_params = MakeEmbedderParams();
  embedder_params->execute_for_gemma = true;
  service()->LoadModels(std::move(model_params), std::move(embedder_params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        load_models_future.GetCallback());
  EXPECT_TRUE(load_models_future.Get());

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future1;
  embedder_remote->GenerateEmbeddings({"hello"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future1.GetCallback());
  EXPECT_EQ(execute_future1.Take().size(), 1u);

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future2;
  embedder_remote->GenerateEmbeddings({"hello"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future2.GetCallback());
  EXPECT_EQ(execute_future2.Take().size(), 1u);

  // No cache checks/puts should occur for Gemma, so the metric is never
  // recorded.
  histogram_tester.ExpectTotalCount("History.Embeddings.Embedder.CacheHit", 0);
}

}  // namespace
}  // namespace passage_embeddings
