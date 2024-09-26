// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gif_tenor_api_fetcher.h"
#include <memory>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom-forward.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kFakeCategoriesResponse[] =
    "{\"tags\":[{\"image\":\"url1\",\"name\":\"#awesome\",\"path\":\"/v2/"
    "path1\","
    "\"searchterm\":\"awesome\"}, "
    "{\"image\":\"url2\",\"name\":\"#jk\",\"path\":\"/v2/"
    "path2\","
    "\"searchterm\":\"jk\"}]}";

constexpr char kFakeGifsResponse[] =
    "{\"next\":\"1\",\"results\":[{\"id\":\"0\","
    "\"content_description\":\"GIF0\",\"media_formats\":{\"gif\":{\"dims\":"
    "["
    "498,339],\"url\":\"https://tenor.com/view/media.tenor.com/"
    "full_url0\"},\"tinygif\":{\"dims\":[220,150]"
    ",\"url\":\"https://tenor.com/view/media.tenor.com/"
    "preview_url0\"}}},{\"id\":\"1\",\"content_description\":\"GIF1\","
    "\"media_formats\":{\"gif\":{\"dims\":[498,498],\"url\":\"https://"
    "tenor.com/view/media.tenor.com/"
    "full_url1\"},\"tinygif\":{\"dims\":[220,220],\"url\":\"https://"
    "tenor.com/view/media.tenor.com/preview_url1\"}}}]}";

std::vector<emoji_picker::mojom::GifResponsePtr> GetFakeGifs() {
  std::vector<emoji_picker::mojom::GifResponsePtr> gifs;
  gifs.push_back(emoji_picker::mojom::GifResponse::New(
      "0", "GIF0",
      emoji_picker::mojom::GifUrls::New(
          GURL("https://tenor.com/view/media.tenor.com/full_url0"),
          GURL("https://tenor.com/view/media.tenor.com/preview_url0")),
      gfx::Size(220, 150)));
  gifs.push_back(emoji_picker::mojom::GifResponse::New(
      "1", "GIF1",
      emoji_picker::mojom::GifUrls::New(
          GURL("https://tenor.com/view/media.tenor.com/full_url1"),
          GURL("https://tenor.com/view/media.tenor.com/preview_url1")),
      gfx::Size(220, 220)));
  return gifs;
}
}  // namespace

class FakeEndpointFetcher : public EndpointFetcher {
 public:
  explicit FakeEndpointFetcher(EndpointResponse response)
      : EndpointFetcher(net::DefineNetworkTrafficAnnotation(
            "chromeos_emoji_picker_mock_fetcher",
            R"()")),
        response_(response) {}

  void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                      const char* key) override {
    std::move(endpoint_fetcher_callback)
        .Run(std::make_unique<EndpointResponse>(response_));
  }

 private:
  EndpointResponse response_;
};

class GifTenorApiFetcherTest : public testing::Test {
 public:
  GifTenorApiFetcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& annotation_tag) {
    return std::make_unique<FakeEndpointFetcher>(response_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  EndpointResponse response_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GifTenorApiFetcher gif_tenor_api_fetcher_ = GifTenorApiFetcher(
      base::BindRepeating(&GifTenorApiFetcherTest::CreateEndpointFetcher,
                          base::Unretained(this)));
};

TEST_F(GifTenorApiFetcherTest, FetchCategories) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<emoji_picker::mojom::Status,
                         const std::vector<std::string>&>
      create_future_http_error;
  gif_tenor_api_fetcher_.FetchCategories(create_future_http_error.GetCallback(),
                                         url_loader_factory_);
  ASSERT_EQ(create_future_http_error.Get<0>(),
            emoji_picker::mojom::Status::kHttpError);
  ASSERT_EQ(create_future_http_error.Get<1>(), std::vector<std::string>{});

  response_.error_type = absl::make_optional(FetchErrorType::kNetError);
  base::test::TestFuture<emoji_picker::mojom::Status,
                         const std::vector<std::string>&>
      create_future_net_error;
  gif_tenor_api_fetcher_.FetchCategories(create_future_net_error.GetCallback(),
                                         url_loader_factory_);
  ASSERT_EQ(create_future_net_error.Get<0>(),
            emoji_picker::mojom::Status::kNetError);
  ASSERT_EQ(create_future_net_error.Get<1>(), std::vector<std::string>{});

  base::test::TestFuture<emoji_picker::mojom::Status,
                         const std::vector<std::string>&>
      create_future_http_ok;
  response_.response = kFakeCategoriesResponse;
  response_.http_status_code = net::HTTP_OK;
  gif_tenor_api_fetcher_.FetchCategories(create_future_http_ok.GetCallback(),
                                         url_loader_factory_);
  std::vector<std::string> expected{"#awesome", "#jk"};
  ASSERT_EQ(create_future_http_ok.Get<0>(),
            emoji_picker::mojom::Status::kHttpOk);
  ASSERT_EQ(create_future_http_ok.Get<1>(), expected);
}

TEST_F(GifTenorApiFetcherTest, FetchFeaturedGifs) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<emoji_picker::mojom::Status,
                         emoji_picker::mojom::TenorGifResponsePtr>
      create_future_http_error;
  gif_tenor_api_fetcher_.FetchFeaturedGifs(
      create_future_http_error.GetCallback(), url_loader_factory_, "");
  ASSERT_EQ(create_future_http_error.Get<0>(),
            emoji_picker::mojom::Status::kHttpError);
  ASSERT_EQ(create_future_http_error.Get<1>(),
            emoji_picker::mojom::TenorGifResponse::New(
                "", std::vector<emoji_picker::mojom::GifResponsePtr>{}));

  response_.error_type = absl::make_optional(FetchErrorType::kNetError);
  base::test::TestFuture<emoji_picker::mojom::Status,
                         emoji_picker::mojom::TenorGifResponsePtr>
      create_future_net_error;
  gif_tenor_api_fetcher_.FetchFeaturedGifs(
      create_future_net_error.GetCallback(), url_loader_factory_, "");
  ASSERT_EQ(create_future_net_error.Get<0>(),
            emoji_picker::mojom::Status::kNetError);
  ASSERT_EQ(create_future_net_error.Get<1>(),
            emoji_picker::mojom::TenorGifResponse::New(
                "", std::vector<emoji_picker::mojom::GifResponsePtr>{}));

  response_.response = kFakeGifsResponse;
  response_.http_status_code = net::HTTP_OK;
  base::test::TestFuture<emoji_picker::mojom::Status,
                         emoji_picker::mojom::TenorGifResponsePtr>
      create_future_http_ok;
  gif_tenor_api_fetcher_.FetchFeaturedGifs(create_future_http_ok.GetCallback(),
                                           url_loader_factory_, "");
  ASSERT_EQ(create_future_http_ok.Get<0>(),
            emoji_picker::mojom::Status::kHttpOk);
  ASSERT_EQ(create_future_http_ok.Get<1>(),
            emoji_picker::mojom::TenorGifResponse::New("1", GetFakeGifs()));
}

TEST_F(GifTenorApiFetcherTest, FetchGifSearch) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<emoji_picker::mojom::Status,
                         emoji_picker::mojom::TenorGifResponsePtr>
      create_future_http_error;
  gif_tenor_api_fetcher_.FetchGifSearch(create_future_http_error.GetCallback(),
                                        url_loader_factory_, "", "");
  ASSERT_EQ(create_future_http_error.Get<0>(),
            emoji_picker::mojom::Status::kHttpError);
  ASSERT_EQ(create_future_http_error.Get<1>(),
            emoji_picker::mojom::TenorGifResponse::New(
                "", std::vector<emoji_picker::mojom::GifResponsePtr>{}));

  response_.error_type = absl::make_optional(FetchErrorType::kNetError);
  base::test::TestFuture<emoji_picker::mojom::Status,
                         emoji_picker::mojom::TenorGifResponsePtr>
      create_future_net_error;
  gif_tenor_api_fetcher_.FetchFeaturedGifs(
      create_future_net_error.GetCallback(), url_loader_factory_, "");
  ASSERT_EQ(create_future_net_error.Get<0>(),
            emoji_picker::mojom::Status::kNetError);
  ASSERT_EQ(create_future_net_error.Get<1>(),
            emoji_picker::mojom::TenorGifResponse::New(
                "", std::vector<emoji_picker::mojom::GifResponsePtr>{}));

  response_.response = kFakeGifsResponse;
  response_.http_status_code = net::HTTP_OK;
  base::test::TestFuture<emoji_picker::mojom::Status,
                         emoji_picker::mojom::TenorGifResponsePtr>
      create_future_http_ok;
  gif_tenor_api_fetcher_.FetchFeaturedGifs(create_future_http_ok.GetCallback(),
                                           url_loader_factory_, "");
  ASSERT_EQ(create_future_http_ok.Get<0>(),
            emoji_picker::mojom::Status::kHttpOk);
  ASSERT_EQ(create_future_http_ok.Get<1>(),
            emoji_picker::mojom::TenorGifResponse::New("1", GetFakeGifs()));
}

TEST_F(GifTenorApiFetcherTest, FetchGifsByIds) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<emoji_picker::mojom::Status,
                         std::vector<emoji_picker::mojom::GifResponsePtr>>
      create_future_http_error;
  gif_tenor_api_fetcher_.FetchGifsByIds(create_future_http_error.GetCallback(),
                                        url_loader_factory_,
                                        std::vector<std::string>());
  ASSERT_EQ(create_future_http_error.Get<0>(),
            emoji_picker::mojom::Status::kHttpError);
  ASSERT_EQ(create_future_http_error.Get<1>(),
            std::vector<emoji_picker::mojom::GifResponsePtr>{});

  response_.error_type = absl::make_optional(FetchErrorType::kNetError);
  base::test::TestFuture<emoji_picker::mojom::Status,
                         std::vector<emoji_picker::mojom::GifResponsePtr>>
      create_future_net_error;
  gif_tenor_api_fetcher_.FetchGifsByIds(create_future_net_error.GetCallback(),
                                        url_loader_factory_,
                                        std::vector<std::string>());
  ASSERT_EQ(create_future_net_error.Get<0>(),
            emoji_picker::mojom::Status::kNetError);
  ASSERT_EQ(create_future_net_error.Get<1>(),
            std::vector<emoji_picker::mojom::GifResponsePtr>{});

  response_.response = kFakeGifsResponse;
  response_.http_status_code = net::HTTP_OK;
  base::test::TestFuture<emoji_picker::mojom::Status,
                         std::vector<emoji_picker::mojom::GifResponsePtr>>
      create_future_http_ok;
  gif_tenor_api_fetcher_.FetchGifsByIds(create_future_http_ok.GetCallback(),
                                        url_loader_factory_,
                                        std::vector<std::string>());
  ASSERT_EQ(create_future_http_ok.Get<0>(),
            emoji_picker::mojom::Status::kHttpOk);
  ASSERT_EQ(create_future_http_ok.Get<1>(), GetFakeGifs());
}

}  // namespace ash
