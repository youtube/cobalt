// Copyright 2021 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_LOADER_IMAGE_IMAGE_DECODER_MOCK_H_
#define COBALT_LOADER_IMAGE_IMAGE_DECODER_MOCK_H_

#include <memory>
#include <string>

#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace image {

struct MockImageDecoderCallback {
  void SuccessCallback(const scoped_refptr<Image>& value) { image = value; }

  MOCK_METHOD1(LoadCompleteCallback,
               void(const base::Optional<std::string>& message));

  scoped_refptr<Image> image;
};

class MockImageDecoder : public Decoder {
 public:
  MockImageDecoder();
  explicit MockImageDecoder(render_tree::ResourceProvider* resource_provider);
  ~MockImageDecoder() override {}

  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override;

  void DecodeChunk(const char* data, size_t size) override;

  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

  scoped_refptr<Image> image();

  void ExpectCallWithError(const base::Optional<std::string>& error);

 protected:
  std::unique_ptr<render_tree::ResourceProviderStub> resource_provider_stub_;
  render_tree::ResourceProvider* resource_provider_;
  base::NullDebuggerHooks debugger_hooks_;
  ::testing::StrictMock<MockImageDecoderCallback> image_decoder_callback_;
  std::unique_ptr<Decoder> image_decoder_;
};

MockImageDecoder::MockImageDecoder()
    : resource_provider_stub_(new render_tree::ResourceProviderStub()),
      resource_provider_(resource_provider_stub_.get()) {
  image_decoder_.reset(new ImageDecoder(
      resource_provider_, debugger_hooks_,
      base::Bind(&MockImageDecoderCallback::SuccessCallback,
                 base::Unretained(&image_decoder_callback_)),
      base::Bind(&MockImageDecoderCallback::LoadCompleteCallback,
                 base::Unretained(&image_decoder_callback_))));
}

MockImageDecoder::MockImageDecoder(
    render_tree::ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  image_decoder_.reset(new ImageDecoder(
      resource_provider_, debugger_hooks_,
      base::Bind(&MockImageDecoderCallback::SuccessCallback,
                 base::Unretained(&image_decoder_callback_)),
      base::Bind(&MockImageDecoderCallback::LoadCompleteCallback,
                 base::Unretained(&image_decoder_callback_))));
}

LoadResponseType MockImageDecoder::OnResponseStarted(
    Fetcher* fetcher, const scoped_refptr<net::HttpResponseHeaders>& headers) {
  return image_decoder_->OnResponseStarted(fetcher, headers);
}

void MockImageDecoder::DecodeChunk(const char* data, size_t size) {
  image_decoder_->DecodeChunk(data, size);
}

void MockImageDecoder::Finish() { image_decoder_->Finish(); }

bool MockImageDecoder::Suspend() { return image_decoder_->Suspend(); }

void MockImageDecoder::Resume(
    render_tree::ResourceProvider* resource_provider) {
  image_decoder_->Resume(resource_provider);
}

scoped_refptr<Image> MockImageDecoder::image() {
  return image_decoder_callback_.image;
}

void MockImageDecoder::ExpectCallWithError(
    const base::Optional<std::string>& error) {
  EXPECT_CALL(image_decoder_callback_, LoadCompleteCallback(error));
}
}  // namespace image
}  // namespace loader
};  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_DECODER_MOCK_H_
