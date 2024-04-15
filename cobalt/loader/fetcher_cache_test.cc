// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/fetcher_cache.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cobalt/loader/fetcher.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace loader {
namespace {

constexpr int kFetcherCapacity = 1024 * 1024;

class StubFetcher : public Fetcher {
 public:
  explicit StubFetcher(Handler* handler) : Fetcher(handler) {}
  ~StubFetcher() override {}

  void FireResponseStarted(
      const scoped_refptr<net::HttpResponseHeaders>& headers) {
    auto response_type = handler()->OnResponseStarted(this, headers);
    if (response_type == kLoadResponseAbort) {
      return;
    }
  }

  void FireReceived(const char* data, size_t size) {
    handler()->OnReceived(this, data, size);
  }

  void FireDone() { handler()->OnDone(this); }

  void FireFetcherProcess(
      const scoped_refptr<net::HttpResponseHeaders>& headers, const char* data,
      size_t size) {
    LoadResponseType response = handler()->OnResponseStarted(this, headers);
    if (response == kLoadResponseContinue) {
      handler()->OnReceived(this, data, size);
      handler()->OnDone(this);
    }
  }
};

class StubFetcherHandler : public Fetcher::Handler {
 public:
  // From Fetcher::Handler.
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {}
  void OnDone(Fetcher* fetcher) override {}
  void OnError(Fetcher* fetcher, const std::string& error_message) override {}
};

struct StubFetcherFactory {
 public:
  StubFetcherFactory() {}
  // Way to access the last fetcher created by the fetcher factory.
  StubFetcher* fetcher;

  std::unique_ptr<Fetcher> Create(Fetcher::Handler* handler) {
    fetcher = new StubFetcher(handler);
    return base::WrapUnique<Fetcher>(fetcher);
  }

  Loader::FetcherCreator GetFetcherCreator() {
    return base::Bind(&StubFetcherFactory::Create, base::Unretained(this));
  }
};

std::unique_ptr<Fetcher> CreateFetcher(Fetcher::Handler* handler) {
  StubFetcher* fetcher = new StubFetcher(handler);
  return base::WrapUnique<Fetcher>(fetcher);
}

}  // namespace

class FetcherCacheTest : public ::testing::Test {
 protected:
  class OngoingFetcherFactory {
   public:
    OngoingFetcherFactory(const char* name, size_t capacity) {
      fetcher_cache_ = base::MakeRefCounted<FetcherCache>(name, capacity);
    }
    ~OngoingFetcherFactory() {
      if (fetcher_cache_) {
        fetcher_cache_->DestroySoon();
      }
    }

    size_t size() const { return fetcher_cache_->size(); }

    size_t capacity() const { return fetcher_cache_->capacity(); }

    Loader::FetcherCreator MakeCachedFetcherCreator(
        const GURL& url, const Loader::FetcherCreator& real_fetcher_creator) {
      return fetcher_cache_->GetFetcherCreator(url, real_fetcher_creator);
    }

   private:
    scoped_refptr<FetcherCache> fetcher_cache_;
  };

  const std::string data_ = "Test string! Test string! Test!!";
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  OngoingFetcherFactory fetcher_factory_{"cache_name", kFetcherCapacity};
};

TEST_F(FetcherCacheTest, CtorAndDtor) {
  EXPECT_EQ(kFetcherCapacity, fetcher_factory_.capacity());
}

TEST_F(FetcherCacheTest, MakeFetcher) {
  auto fetcher_creator = fetcher_factory_.MakeCachedFetcherCreator(
      GURL("https://example.com"), base::Bind(&CreateFetcher));
  EXPECT_EQ(0, fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, SunnyDay) {
  StubFetcherHandler handler;
  StubFetcherFactory stub_fetcher_factory;
  auto cb = stub_fetcher_factory.GetFetcherCreator();

  auto fetcher_creator = fetcher_factory_.MakeCachedFetcherCreator(
      GURL("https://example.com"), cb);
  auto fetcher = fetcher_creator.Run(&handler);

  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory.fetcher),
                                data_.c_str(), data_.size()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory.fetcher)));

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_.capacity(), fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, DuplicateFetcherCache) {
  StubFetcherHandler handler;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

  std::vector<StubFetcherFactory> stub_fetcher_factorys;
  std::vector<Loader::FetcherCreator> fetcher_creators;
  std::vector<std::unique_ptr<Fetcher>> fetchers;

  for (size_t i = 0; i < 2; i++) {
    StubFetcherFactory stub_fetcher_factory;
    Loader::FetcherCreator fetcher_creator =
        fetcher_factory_.MakeCachedFetcherCreator(
            GURL("https://example.com"),
            stub_fetcher_factory.GetFetcherCreator());
    std::unique_ptr<Fetcher> fetcher = fetcher_creator.Run(&handler);

    stub_fetcher_factorys.push_back(stub_fetcher_factory);
    fetcher_creators.push_back(fetcher_creator);
    fetchers.push_back(std::move(fetcher));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&StubFetcher::FireFetcherProcess,
                       base::Unretained(stub_fetcher_factorys[i].fetcher),
                       headers, data_.c_str(), data_.size()));
  }

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_.capacity(), fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, MultipleFetcherCache) {
  StubFetcherHandler handler;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  const size_t kMaxNumCache = 10;

  std::vector<StubFetcherFactory> stub_fetcher_factorys;
  std::vector<Loader::FetcherCreator> fetcher_creators;
  std::vector<std::unique_ptr<Fetcher>> fetchers;

  for (size_t i = 0; i < kMaxNumCache; i++) {
    StubFetcherFactory stub_fetcher_factory;
    Loader::FetcherCreator fetcher_creator =
        fetcher_factory_.MakeCachedFetcherCreator(
            GURL("https://example.com" + base::NumberToString(i)),
            stub_fetcher_factory.GetFetcherCreator());
    std::unique_ptr<Fetcher> fetcher = fetcher_creator.Run(&handler);

    stub_fetcher_factorys.push_back(stub_fetcher_factory);
    fetcher_creators.push_back(fetcher_creator);
    fetchers.push_back(std::move(fetcher));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&StubFetcher::FireFetcherProcess,
                       base::Unretained(stub_fetcher_factorys[i].fetcher),
                       headers, data_.c_str(), data_.size()));
  }

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_.capacity() * kMaxNumCache, fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, MaxFetcherCache) {
  const size_t capacity = kFetcherCapacity;
  const size_t data_capacity = data_.capacity();
  StubFetcherHandler handler;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  // kMaxNumCache = ceil(capacity / data_.capacity()) + 1, this will force
  // to write a new cache and delete an old cache
  const size_t kMaxNumCache = 1 + (capacity / data_capacity);

  std::vector<StubFetcherFactory> stub_fetcher_factorys;
  std::vector<Loader::FetcherCreator> fetcher_creators;
  std::vector<std::unique_ptr<Fetcher>> fetchers;

  for (size_t i = 0; i < kMaxNumCache; i++) {
    StubFetcherFactory stub_fetcher_factory;
    Loader::FetcherCreator fetcher_creator =
        fetcher_factory_.MakeCachedFetcherCreator(
            GURL("https://example.com" + base::NumberToString(i)),
            stub_fetcher_factory.GetFetcherCreator());
    std::unique_ptr<Fetcher> fetcher = fetcher_creator.Run(&handler);

    stub_fetcher_factorys.push_back(stub_fetcher_factory);
    fetcher_creators.push_back(fetcher_creator);
    fetchers.push_back(std::move(fetcher));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&StubFetcher::FireFetcherProcess,
                       base::Unretained(stub_fetcher_factorys[i].fetcher),
                       headers, data_.c_str(), data_.size()));
  }

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_capacity * (kMaxNumCache - 1), fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, DisorderedSameFetcherCache) {
  StubFetcherHandler handler;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

  StubFetcherFactory stub_fetcher_factory1, stub_fetcher_factory2;
  Loader::FetcherCreator fetcher_creator1 =
      fetcher_factory_.MakeCachedFetcherCreator(
          GURL("https://example.com"),
          stub_fetcher_factory1.GetFetcherCreator());
  std::unique_ptr<Fetcher> fetcher1 = fetcher_creator1.Run(&handler);
  Loader::FetcherCreator fetcher_creator2 =
      fetcher_factory_.MakeCachedFetcherCreator(
          GURL("https://example.com"),
          stub_fetcher_factory2.GetFetcherCreator());
  std::unique_ptr<Fetcher> fetcher2 = fetcher_creator2.Run(&handler);

  // Task 1: start and receive
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory1.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory1.fetcher),
                                data_.c_str(), data_.size()));
  // Task 2: start and receive
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory2.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory2.fetcher),
                                data_.c_str(), data_.size()));
  // Task 1: done
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory1.fetcher)));
  // Task 2: done
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory2.fetcher)));

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_.capacity(), fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, DisorderedDiffFetcherCache) {
  StubFetcherHandler handler;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

  StubFetcherFactory stub_fetcher_factory1, stub_fetcher_factory2;
  Loader::FetcherCreator fetcher_creator1 =
      fetcher_factory_.MakeCachedFetcherCreator(
          GURL("https://example1.com"),
          stub_fetcher_factory1.GetFetcherCreator());
  std::unique_ptr<Fetcher> fetcher1 = fetcher_creator1.Run(&handler);
  Loader::FetcherCreator fetcher_creator2 =
      fetcher_factory_.MakeCachedFetcherCreator(
          GURL("https://example2.com"),
          stub_fetcher_factory2.GetFetcherCreator());
  std::unique_ptr<Fetcher> fetcher2 = fetcher_creator2.Run(&handler);

  // Task 1: start and receive
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory1.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory1.fetcher),
                                data_.c_str(), data_.size()));
  // Task 2: start and receive
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory2.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory2.fetcher),
                                data_.c_str(), data_.size()));
  // Task 1: done
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory1.fetcher)));
  // Task 2: done
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory2.fetcher)));

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_.capacity() * 2, fetcher_factory_.size());
}

TEST_F(FetcherCacheTest, DisorderedSlowFetcherCache) {
  StubFetcherHandler handler;
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

  StubFetcherFactory stub_fetcher_factory1, stub_fetcher_factory2;
  Loader::FetcherCreator fetcher_creator1 =
      fetcher_factory_.MakeCachedFetcherCreator(
          GURL("https://example1.com"),
          stub_fetcher_factory1.GetFetcherCreator());
  std::unique_ptr<Fetcher> fetcher1 = fetcher_creator1.Run(&handler);
  Loader::FetcherCreator fetcher_creator2 =
      fetcher_factory_.MakeCachedFetcherCreator(
          GURL("https://example2.com"),
          stub_fetcher_factory2.GetFetcherCreator());
  std::unique_ptr<Fetcher> fetcher2 = fetcher_creator2.Run(&handler);

  // Task 1: start and receive
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory1.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory1.fetcher),
                                data_.c_str(), data_.size()));
  // Task 2: start, receive, and done
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireResponseStarted,
                     base::Unretained(stub_fetcher_factory2.fetcher), headers));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StubFetcher::FireReceived,
                                base::Unretained(stub_fetcher_factory2.fetcher),
                                data_.c_str(), data_.size()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory2.fetcher)));
  // Task 1: done
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StubFetcher::FireDone,
                     base::Unretained(stub_fetcher_factory1.fetcher)));

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(data_.capacity() * 2, fetcher_factory_.size());
}

}  // namespace loader
}  // namespace cobalt
