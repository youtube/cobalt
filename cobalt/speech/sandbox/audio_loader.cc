// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/speech/sandbox/audio_loader.h"

#include <vector>

namespace cobalt {
namespace speech {
namespace sandbox {

namespace {
class DummyDecoder : public loader::Decoder {
 public:
  typedef base::Callback<void(const uint8*, int)> DoneCallback;

  explicit DummyDecoder(const DoneCallback& done_callback)
      : done_callback_(done_callback) {}
  ~DummyDecoder() override {}

  // This function is used for binding callback for creating DummyDecoder.
  static scoped_ptr<Decoder> Create(const DoneCallback& done_callback) {
    return scoped_ptr<Decoder>(new DummyDecoder(done_callback));
  }

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    // Because we load data into memory, set a maximum buffer size.
    const int kMaxBufferSize = 1024 * 1024;
    if (buffer_.size() + size > kMaxBufferSize) {
      return;
    }
    buffer_.insert(buffer_.end(), data, data + size);
  }

  void Finish() override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (buffer_.size() == 0) {
      // No data loaded.
      done_callback_.Run(NULL, 0);
      return;
    }

    done_callback_.Run(reinterpret_cast<uint8*>(&buffer_[0]), buffer_.size());
  }
  bool Suspend() override {
    NOTIMPLEMENTED();
    return false;
  }
  void Resume(render_tree::ResourceProvider* /*resource_provider*/) override {
    NOTIMPLEMENTED();
  }

 private:
  base::ThreadChecker thread_checker_;
  std::vector<char> buffer_;
  DoneCallback done_callback_;
};
}  // namespace

AudioLoader::AudioLoader(const GURL& url,
                         network::NetworkModule* network_module,
                         const DoneCallback& callback)
    : network_module_(network_module), done_callback_(callback) {
  DCHECK(url.is_valid());
  DCHECK(!callback.is_null());

  fetcher_factory_.reset(new loader::FetcherFactory(network_module_));
  loader_ = make_scoped_ptr(new loader::Loader(
      base::Bind(&loader::FetcherFactory::CreateFetcher,
                 base::Unretained(fetcher_factory_.get()), url),
      scoped_ptr<loader::Decoder>(new DummyDecoder(
          base::Bind(&AudioLoader::OnLoadingDone, base::Unretained(this)))),
      base::Bind(&AudioLoader::OnLoadingError, base::Unretained(this))));
}

AudioLoader::~AudioLoader() {}

void AudioLoader::OnLoadingDone(const uint8* data, int size) {
  done_callback_.Run(data, size);
}

void AudioLoader::OnLoadingError(const std::string& error) {
  DLOG(WARNING) << "OnLoadingError with error message: " << error;
}

}  // namespace sandbox
}  // namespace speech
}  // namespace cobalt
