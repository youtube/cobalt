// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/server_printers_provider.h"

#include <map>
#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/printing/print_servers_provider.h"
#include "chrome/browser/ash/printing/print_servers_provider_factory.h"
#include "chrome/browser/ash/printing/server_printers_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class PrefService;

namespace ash {

namespace {

// Internal structure representing print server.
struct PrintServerWithPrinters {
  explicit PrintServerWithPrinters(const PrintServer& ps) : server(ps) {}

  PrintServer server;
  std::vector<PrinterDetector::DetectedPrinter> printers;  // queried printers
};

class ServerPrintersProviderImpl : public ServerPrintersProvider {
 public:
  explicit ServerPrintersProviderImpl(Profile* profile) : profile_(profile) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  ServerPrintersProviderImpl(const ServerPrintersProviderImpl&) = delete;
  ServerPrintersProviderImpl& operator=(const ServerPrintersProviderImpl&) =
      delete;

  ~ServerPrintersProviderImpl() override = default;

  void RegisterPrintersFoundCallback(OnPrintersUpdateCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    callback_ = std::move(cb);
  }

  std::vector<PrinterDetector::DetectedPrinter> GetPrinters() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<PrinterDetector::DetectedPrinter> printers;
    for (auto& server : servers_) {
      printers.insert(printers.end(), server.second.printers.begin(),
                      server.second.printers.end());
    }
    return printers;
  }

  void OnServersChanged(bool servers_are_complete,
                        const std::map<GURL, PrintServer>& servers) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Save previous state.
    const bool previous_complete = IsComplete();
    // Initialize new state.
    servers_are_complete_ = servers_are_complete;
    // Fill new map with new servers and compare with the old map.
    std::map<GURL, PrintServerWithPrinters> new_servers;
    for (const auto& server_pair : servers) {
      const PrintServer& server = server_pair.second;
      const GURL& url = server.GetUrl();
      const std::string& name = server.GetName();
      auto it_new = new_servers.emplace(url, server).first;
      auto it_old = servers_.find(url);
      if (it_old != servers_.end()) {
        // This server already exists: copy content and erase it from the
        // old map.
        it_new->second.printers = std::move(it_old->second.printers);
        servers_.erase(it_old);
      } else {
        // This is a new print server: query for printers.
        fetchers_.emplace(
            url, std::make_unique<ServerPrintersFetcher>(
                     profile_, url, name,
                     base::BindRepeating(
                         &ServerPrintersProviderImpl::OnPrintersFetched,
                         weak_ptr_factory_.GetWeakPtr())));
      }
    }
    // The rest of servers in the old map are going to be deleted.
    // Check if there are any server printers or fetchers to delete.
    bool change_in_printers = false;
    for (const auto& server : servers_) {
      if (!server.second.printers.empty()) {
        change_in_printers = true;
      }
      fetchers_.erase(server.first);
    }
    // Replace the old map of servers with the new one.
    servers_ = std::move(new_servers);
    // Notify the observer if something changed.
    if (callback_) {
      const bool current_complete = IsComplete();
      if (change_in_printers || (previous_complete != current_complete)) {
        callback_.Run(current_complete);
      }
    }
  }

  // A callback from ServerPrintersFetcher.
  void OnPrintersFetched(
      const ServerPrintersFetcher* sender,
      const GURL& server_url,
      std::vector<PrinterDetector::DetectedPrinter>&& printers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const bool previous_complete = IsComplete();
    auto it = fetchers_.find(server_url);
    // Do nothing if the fetcher is obsolete.
    if (it == fetchers_.end() || it->second.get() != sender) {
      return;
    }
    // Remove the fetcher from the list.
    fetchers_.erase(it);
    // When old and new printers are empty and there is no change in
    // completeness status we leave here.
    DCHECK(base::Contains(servers_, server_url));
    if (servers_.at(server_url).printers.empty() && printers.empty() &&
        previous_complete == IsComplete()) {
      return;
    }
    // Save printers.
    servers_.at(server_url).printers = printers;
    // Notify the observer.
    if (callback_) {
      callback_.Run(IsComplete());
    }
  }

 private:
  // Returns true <=> all policies have been parsed and applied and all servers
  // have been queried (even when some errors occurred).
  bool IsComplete() const {
    return (servers_are_complete_ && fetchers_.empty());
  }

  raw_ptr<Profile, ExperimentalAsh> profile_;

  // A callback to propagate update of the resultant list of server printers.
  OnPrintersUpdateCallback callback_;

  // True <=> the list of print servers is complete.
  bool servers_are_complete_ = false;

  // All print servers (with printers).
  std::map<GURL, PrintServerWithPrinters> servers_;

  // URLs that are being queried now with corresponding fetcher objects.
  std::map<GURL, std::unique_ptr<ServerPrintersFetcher>> fetchers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ServerPrintersProviderImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<ServerPrintersProvider> ServerPrintersProvider::Create(
    Profile* profile) {
  return std::make_unique<ServerPrintersProviderImpl>(profile);
}

}  // namespace ash
