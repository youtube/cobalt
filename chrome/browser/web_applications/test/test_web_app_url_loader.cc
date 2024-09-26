// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"

namespace web_app {

TestWebAppUrlLoader::TestWebAppUrlLoader() = default;

TestWebAppUrlLoader::~TestWebAppUrlLoader() = default;

void TestWebAppUrlLoader::SaveLoadUrlRequests() {
  should_save_requests_ = true;
}

void TestWebAppUrlLoader::ProcessLoadUrlRequests() {
  while (!pending_requests_.empty()) {
    auto [url, callback] = std::move(pending_requests_.front());
    pending_requests_.pop();

    DCHECK(base::Contains(next_result_map_, url));

    const UrlResponses& url_responses = next_result_map_[url];
    DCHECK_EQ(1u, url_responses.results.size());

    Result result = url_responses.results.front();
    next_result_map_.erase(url);

    std::move(callback).Run(result);
  }
}

void TestWebAppUrlLoader::SetNextLoadUrlResult(const GURL& url, Result result) {
  AddNextLoadUrlResults(url, {result});
}

void TestWebAppUrlLoader::AddNextLoadUrlResults(
    const GURL& url,
    const std::vector<Result>& results) {
  DCHECK(!base::Contains(next_result_map_, url)) << url;
  UrlResponses& responses = next_result_map_[url];

  for (Result result : results)
    responses.results.push(result);
}

void TestWebAppUrlLoader::LoadUrl(const GURL& url,
                                  content::WebContents* web_contents,
                                  UrlComparison url_comparison,
                                  ResultCallback callback) {
  load_url_tracker_.Run(url, web_contents, url_comparison);

  if (should_save_requests_) {
    pending_requests_.emplace(url, std::move(callback));
    return;
  }

  DCHECK(base::Contains(next_result_map_, url)) << url;
  UrlResponses& responses = next_result_map_[url];
  DCHECK(!responses.results.empty());

  Result result = responses.results.front();
  responses.results.pop();

  if (responses.results.empty())
    next_result_map_.erase(url);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void TestWebAppUrlLoader::SetPrepareForLoadResultLoaded() {
  AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
}

void TestWebAppUrlLoader::AddPrepareForLoadResults(
    const std::vector<Result>& results) {
  AddNextLoadUrlResults(GURL(url::kAboutBlankURL), results);
}

TestWebAppUrlLoader::UrlResponses::UrlResponses() = default;

TestWebAppUrlLoader::UrlResponses::~UrlResponses() = default;

}  // namespace web_app
