// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/url_downloader.h"

#import <string>
#import <vector>

#import "base/base64.h"
#import "base/containers/contains.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/json/json_writer.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/task/thread_pool.h"
#import "components/reading_list/core/offline_url_utils.h"
#import "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/reading_list/reading_list_distiller_page.h"
#import "ios/chrome/browser/reading_list/reading_list_distiller_page_factory.h"
#import "net/base/load_flags.h"
#import "net/base/mime_sniffer.h"
#import "net/http/http_response_headers.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "services/network/public/mojom/url_response_head.mojom.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// This script disables context menu on img elements.
// The pages are stored locally and long pressing on them will trigger a context
// menu on the file:// URL which cannot be opened. Disable the context menu.
const char kDisableImageContextMenuScript[] =
    "<script nonce=\"$1\">"
    "document.addEventListener('DOMContentLoaded', function (event) {"
    "    var imgMenuDisabler = document.createElement('style');"
    "    imgMenuDisabler.innerHTML = 'img { -webkit-touch-callout: none; }';"
    "    document.head.appendChild(imgMenuDisabler);"
    "}, false);"
    "</script>";

// This script replaces any downloaded images with a data uri.
const char kReplaceDownloadedImagesScript[] =
    "<script nonce=\"$1\">"
    "document.addEventListener('DOMContentLoaded', function (event) {"
    "    var imgData = {};"
    "    $2"
    "    var imgTags = document.getElementsByTagName(\"img\");"
    "    for(image of imgTags) {"
    "        image.src = imgData[image.src] || image.src;"
    "    }"
    "}, false);"
    "</script>";

// The maximum size for the distilled page.
// Note that the sum of the size of the resources will be used for this check,
// so the total size of the page after processing can be slightly more than
// this.
const int kMaximumTotalPageSize = 10 * 1024 * 1024;

// The maximum size for a single raw image. If a bigger image is found, the
// page distillation is canceled (page will only be available online).
const int kMaximumImageSize = 1024 * 1024;

}  // namespace

// URLDownloader

URLDownloader::URLDownloader(
    dom_distiller::DistillerFactory* distiller_factory,
    reading_list::ReadingListDistillerPageFactory* distiller_page_factory,
    PrefService* prefs,
    base::FilePath chrome_profile_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const DownloadCompletion& download_completion,
    const SuccessCompletion& delete_completion)
    : distiller_page_factory_(distiller_page_factory),
      distiller_factory_(distiller_factory),
      pref_service_(prefs),
      download_completion_(download_completion),
      delete_completion_(delete_completion),
      working_(false),
      base_directory_(chrome_profile_path),
      mime_type_(),
      url_loader_factory_(std::move(url_loader_factory)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      task_tracker_() {}

URLDownloader::~URLDownloader() {
  task_tracker_.TryCancelAll();
}

void URLDownloader::OfflinePathExists(const base::FilePath& path,
                                      base::OnceCallback<void(bool)> callback) {
  task_tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(&base::PathExists, path),
      std::move(callback));
}

void URLDownloader::RemoveOfflineURL(const GURL& url) {
  // Remove all download tasks for this url as it would be pointless work.
  CancelDownloadOfflineURL(url);
  tasks_.push_back(std::make_pair(DELETE, url));
  HandleNextTask();
}

void URLDownloader::DownloadOfflineURL(const GURL& url) {
  if (!base::Contains(tasks_, std::make_pair(DOWNLOAD, url))) {
    tasks_.push_back(std::make_pair(DOWNLOAD, url));
    HandleNextTask();
  }
}

void URLDownloader::CancelDownloadOfflineURL(const GURL& url) {
  tasks_.erase(
      std::remove(tasks_.begin(), tasks_.end(), std::make_pair(DOWNLOAD, url)),
      tasks_.end());
}

void URLDownloader::DownloadCompletionHandler(
    const GURL& url,
    const std::string& title,
    const base::FilePath& offline_path,
    SuccessState success) {
  DCHECK(working_);

  auto post_delete = base::BindOnce(
      [](URLDownloader* _this, const GURL& url, const std::string& title,
         const base::FilePath& offline_path, SuccessState success) {
        _this->download_completion_.Run(url, _this->distilled_url_, success,
                                        offline_path, _this->saved_size_,
                                        title);
        _this->distiller_.reset();
        _this->working_ = false;
        _this->HandleNextTask();
      },
      base::Unretained(this), url, title, offline_path, success);

  // If downloading failed, clean up any partial download.
  if (success == ERROR) {
    base::FilePath directory_path =
        reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);
    task_tracker_.PostTaskAndReply(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& offline_directory_path) {
              base::DeletePathRecursively(offline_directory_path);
            },
            directory_path),
        std::move(post_delete));
  } else {
    std::move(post_delete).Run();
  }
}

void URLDownloader::DeleteCompletionHandler(const GURL& url, bool success) {
  DCHECK(working_);
  delete_completion_.Run(url, success);
  working_ = false;
  HandleNextTask();
}

void URLDownloader::HandleNextTask() {
  if (working_ || tasks_.empty()) {
    return;
  }
  working_ = true;

  Task task = tasks_.front();
  tasks_.pop_front();
  GURL url = task.second;
  base::FilePath directory_path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);

  if (task.first == DELETE) {
    task_tracker_.PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&base::DeletePathRecursively, directory_path),
        base::BindOnce(&URLDownloader::DeleteCompletionHandler,
                       base::Unretained(this), url));
  } else if (task.first == DOWNLOAD) {
    DCHECK(!distiller_);
    OfflinePathExists(directory_path,
                      base::BindOnce(&URLDownloader::DownloadURL,
                                     base::Unretained(this), url));
  }
}

void URLDownloader::DownloadURL(const GURL& url, bool offline_url_exists) {
  if (offline_url_exists) {
    DownloadCompletionHandler(url, std::string(), base::FilePath(),
                              DOWNLOAD_EXISTS);
    return;
  }

  original_url_ = url;
  distilled_url_ = url;
  saved_size_ = 0;
  std::unique_ptr<reading_list::ReadingListDistillerPage>
      reading_list_distiller_page =
          distiller_page_factory_->CreateReadingListDistillerPage(url, this);

  distiller_.reset(new dom_distiller::DistillerViewer(
      distiller_factory_, std::move(reading_list_distiller_page), pref_service_,
      url,
      base::BindRepeating(&URLDownloader::DistillerCallback,
                          base::Unretained(this))));
}

void URLDownloader::DistilledPageRedirectedToURL(const GURL& page_url,
                                                 const GURL& redirected_url) {
  DCHECK(original_url_ == page_url);
  distilled_url_ = redirected_url;
}

void URLDownloader::DistilledPageHasMimeType(const GURL& original_url,
                                             const std::string& mime_type) {
  DCHECK(original_url_ == original_url);
  mime_type_ = mime_type;
}

void URLDownloader::OnURLLoadComplete(const GURL& original_url,
                                      base::FilePath response_path) {
  // At the moment, only pdf files are downloaded using URLFetcher.
  DCHECK(mime_type_ == "application/pdf");
  base::FilePath path = reading_list::OfflinePagePath(
      original_url_, reading_list::OFFLINE_TYPE_PDF);
  std::string mime_type;
  if (url_loader_->ResponseInfo()) {
    mime_type = url_loader_->ResponseInfo()->mime_type;
  }
  if (response_path.empty() || mime_type != mime_type_) {
    return DownloadCompletionHandler(original_url_, "", path, ERROR);
  }

  task_tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&URLDownloader::SavePDFFile, base::Unretained(this),
                     response_path),
      base::BindOnce(&URLDownloader::DownloadCompletionHandler,
                     base::Unretained(this), original_url, "", path));

  url_loader_.reset();
}

void URLDownloader::CancelTask() {
  task_tracker_.TryCancelAll();
  distiller_.reset();
}

void URLDownloader::FetchPDFFile() {
  const GURL& pdf_url =
      distilled_url_.is_valid() ? distilled_url_ : original_url_;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = pdf_url;
  resource_request->load_flags = net::LOAD_SKIP_CACHE_VALIDATION;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 NO_TRAFFIC_ANNOTATION_YET);
  url_loader_->DownloadToTempFile(
      url_loader_factory_.get(),
      base::BindOnce(&URLDownloader::OnURLLoadComplete, base::Unretained(this),
                     pdf_url));
}

URLDownloader::SuccessState URLDownloader::SavePDFFile(
    const base::FilePath& temporary_path) {
  if (CreateOfflineURLDirectory(original_url_)) {
    base::FilePath path = reading_list::OfflinePagePath(
        original_url_, reading_list::OFFLINE_TYPE_PDF);
    base::FilePath absolute_path =
        reading_list::OfflineURLAbsolutePathFromRelativePath(base_directory_,
                                                             path);

    if (base::Move(temporary_path, absolute_path)) {
      int64_t pdf_file_size;
      base::GetFileSize(absolute_path, &pdf_file_size);
      saved_size_ += pdf_file_size;
      return DOWNLOAD_SUCCESS;
    } else {
      return ERROR;
    }
  }

  return ERROR;
}

void URLDownloader::DistillerCallback(
    const GURL& page_url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images,
    const std::string& title) {
  if (html.empty()) {
    // The page may not be HTML. Check the mime-type to see if another handler
    // can save offline content.
    if (mime_type_ == "application/pdf") {
      // PDF handler just downloads the PDF file.
      FetchPDFFile();
      return;
    }
    // This content cannot be processed, return an error value to the client.
    DownloadCompletionHandler(page_url, std::string(), base::FilePath(), ERROR);
    return;
  }

  task_tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&URLDownloader::SaveDistilledHTML, base::Unretained(this),
                     page_url, images, html),
      base::BindOnce(&URLDownloader::DownloadCompletionHandler,
                     base::Unretained(this), page_url, title,
                     reading_list::OfflinePagePath(
                         page_url, reading_list::OFFLINE_TYPE_HTML)));
}

URLDownloader::SuccessState URLDownloader::SaveDistilledHTML(
    const GURL& url,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images,
    const std::string& html) {
  int total_size = html.size();
  for (size_t i = 0; i < images.size(); i++) {
    if (images[i].data.size() > kMaximumImageSize) {
      UMA_HISTOGRAM_MEMORY_KB("IOS.ReadingList.ImageTooLargeFailure",
                              images[i].data.size() / 1024);
      return PERMANENT_ERROR;
    }
    // Image will be base64 encoded.
    total_size += 4 * images[i].data.size() / 3;
  }
  if (total_size > kMaximumTotalPageSize) {
    UMA_HISTOGRAM_MEMORY_KB("IOS.ReadingList.PageTooLargeFailure",
                            total_size / 1024);
    return PERMANENT_ERROR;
  }

  if (CreateOfflineURLDirectory(url)) {
    return SaveHTMLForURL(ReplaceImagesInHTML(url, html, images), url)
               ? DOWNLOAD_SUCCESS
               : ERROR;
  }
  return ERROR;
}

bool URLDownloader::CreateOfflineURLDirectory(const GURL& url) {
  base::FilePath directory_path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);
  if (!DirectoryExists(directory_path)) {
    return CreateDirectoryAndGetError(directory_path, nil);
  }
  return true;
}

std::string URLDownloader::ReplaceImagesInHTML(
    const GURL& url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images) {
  std::string mutable_html = html;
  std::string image_js;
  bool local_images_found = false;
  for (size_t i = 0; i < images.size(); i++) {
    if (images[i].url.SchemeIs(url::kDataScheme)) {
      // Data URI, the data part of the image is empty, no need to store it.
      continue;
    }
    std::string local_image_name;
    // Mixed content is HTTP images on HTTPS pages.
    bool image_is_mixed_content = distilled_url_.SchemeIsCryptographic() &&
                                  !images[i].url.SchemeIsCryptographic();
    // Only inline images if it is not mixed content and image data is valid.
    if (image_is_mixed_content || !images[i].url.is_valid() ||
        images[i].data.empty()) {
      continue;
    }

    // Try to detect the mime-type from the bytes so an arbitrary page cannot
    // be included. Returned mime-type must start with "image/".
    std::string sniffed_type;
    if (!net::SniffMimeTypeFromLocalData(images[i].data, &sniffed_type)) {
      continue;
    }

    if (!base::StartsWith(sniffed_type, "image/")) {
      continue;
    }

    std::string image_url;
    std::string image_data;
    base::Value value(images[i].url.spec());

    base::JSONWriter::Write(value, &image_url);
    base::Base64Encode(images[i].data, &image_data);

    std::string src_with_data =
        base::StringPrintf("data:image/png;base64,%s", image_data.c_str());
    image_js += "imgData[" + image_url + "] = \"" + src_with_data + "\";";

    local_images_found = true;
  }

  if (local_images_found) {
    std::vector<std::string> substitutions;
    substitutions.push_back(distiller_->GetCspNonce());

    mutable_html += base::ReplaceStringPlaceholders(
        kDisableImageContextMenuScript, substitutions, nullptr);

    substitutions.push_back(image_js);
    mutable_html += base::ReplaceStringPlaceholders(
        kReplaceDownloadedImagesScript, substitutions, nullptr);
  }

  return mutable_html;
}

bool URLDownloader::SaveHTMLForURL(std::string html, const GURL& url) {
  if (html.empty()) {
    return false;
  }
  base::FilePath path = reading_list::OfflineURLAbsolutePathFromRelativePath(
      base_directory_,
      reading_list::OfflinePagePath(url, reading_list::OFFLINE_TYPE_HTML));
  if (!base::WriteFile(path, html)) {
    return false;
  }
  saved_size_ += html.size();
  return true;
}
