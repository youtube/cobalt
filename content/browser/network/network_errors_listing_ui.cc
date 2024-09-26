// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_errors_listing_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "content/grit/dev_ui_content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_util.h"

static const char kNetworkErrorDataFile[] = "network-error-data.json";
static const char kErrorCodeField[] = "errorCode";
static const char kErrorCodesDataName[] = "errorCodes";
static const char kErrorIdField[] = "errorId";
static const char kNetworkErrorKey[] = "netError";

namespace content {

namespace {

base::Value::List GetNetworkErrorData() {
  base::Value::Dict error_codes = net::GetNetConstants();
  const base::Value::Dict* net_error_codes_dict =
      error_codes.FindDict(kNetworkErrorKey);
  DCHECK(net_error_codes_dict);

  base::Value::List error_list;

  for (auto it = net_error_codes_dict->begin();
       it != net_error_codes_dict->end(); ++it) {
    const int error_code = it->second.GetInt();
    // Exclude the aborted and pending codes as these don't return a page.
    if (error_code != net::Error::ERR_IO_PENDING &&
        error_code != net::Error::ERR_ABORTED) {
      base::Value::Dict error;
      error.Set(kErrorIdField, error_code);
      error.Set(kErrorCodeField, it->first);
      error_list.Append(std::move(error));
    }
  }
  return error_list;
}

bool ShouldHandleWebUIRequestCallback(const std::string& path) {
  return path == kNetworkErrorDataFile;
}

void HandleWebUIRequestCallback(BrowserContext* current_context,
                                const std::string& path,
                                WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleWebUIRequestCallback(path));

  base::Value::Dict data;
  data.Set(kErrorCodesDataName, GetNetworkErrorData());
  std::string json_string;
  base::JSONWriter::Write(data, &json_string);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(json_string)));
}

}  // namespace

NetworkErrorsListingUI::NetworkErrorsListingUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://network-errors source.
  WebUIDataSource* html_source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUINetworkErrorsListingHost);

  // Add required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePath("network_errors_listing.css",
                               IDR_NETWORK_ERROR_LISTING_CSS);
  html_source->AddResourcePath("network_errors_listing.js",
                               IDR_NETWORK_ERROR_LISTING_JS);
  html_source->SetDefaultResource(IDR_NETWORK_ERROR_LISTING_HTML);
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleWebUIRequestCallback),
      base::BindRepeating(&HandleWebUIRequestCallback,
                          web_ui->GetWebContents()->GetBrowserContext()));
}

}  // namespace content
