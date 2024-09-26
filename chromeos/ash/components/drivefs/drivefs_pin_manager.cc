// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <type_traits>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace drivefs::pinning {
namespace {

using base::SequencedTaskRunner;
using base::TimeDelta;
using mojom::FileMetadata;
using mojom::FileMetadataPtr;
using mojom::QueryItem;
using mojom::QueryItemPtr;
using mojom::ShortcutDetails;
using std::ostream;
using Path = PinManager::Path;
using LookupStatus = ShortcutDetails::LookupStatus;

int Percentage(const int64_t a, const int64_t b) {
  DCHECK_GE(a, 0);
  DCHECK_LE(a, b);
  return b ? 100 * a / b : 100;
}

// Calls the spaced daemon.
void GetFreeSpace(const Path& path, PinManager::SpaceResult callback) {
  ash::SpacedClient* const spaced = ash::SpacedClient::Get();
  DCHECK(spaced);
  spaced->GetFreeDiskSpace(path.value(),
                           base::BindOnce(
                               [](PinManager::SpaceResult callback,
                                  const absl::optional<int64_t> space) {
                                 std::move(callback).Run(space.value_or(-1));
                               },
                               std::move(callback)));
}

class NumPunct : public std::numpunct<char> {
 private:
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};

// Returns a locale that prints numbers with thousands separators.
std::locale NiceNumLocale() {
  static const base::NoDestructor<std::locale> with_separators(
      std::locale::classic(), new NumPunct);
  return *with_separators;
}

template <typename T>
struct Quoter {
  const T& value;
};

template <typename T>
Quoter<T> Quote(const T& value) {
  return {value};
}

template <typename T>
  requires std::is_enum_v<T>
ostream& operator<<(ostream& out, Quoter<T> q) {
  // Convert enum value to string.
  const std::string s = (std::ostringstream() << q.value).str();

  // Does the string start with 'k'?
  if (!s.empty() && s.front() == 'k') {
    // Skip the 'k' prefix.
    return out << base::StringPiece(s).substr(1);
  }

  // No 'k' prefix. Print between parentheses.
  return out << '(' << s << ')';
}

ostream& operator<<(ostream& out, Quoter<TimeDelta> q) {
  const int64_t ms = q.value.InMilliseconds();
  if (ms < 1000) {
    return out << ms << " ms";
  }

  const double seconds = ms / 1000.0;
  if (seconds < 60) {
    return out << base::StringPrintf("%.1f seconds", seconds);
  }

  const double minutes = seconds / 60.0;
  if (minutes < 60) {
    return out << base::StringPrintf("%.1f minutes", minutes);
  }

  const double hours = minutes / 60.0;
  return out << base::StringPrintf("%.1f hours", hours);
}

ostream& operator<<(ostream& out, Quoter<Path> q) {
  return out << "'" << q.value << "'";
}

ostream& operator<<(ostream& out, Quoter<std::string> q) {
  return out << "'" << q.value << "'";
}

template <typename T>
ostream& operator<<(ostream& out, Quoter<absl::optional<T>> q) {
  if (!q.value.has_value()) {
    return out << "(nullopt)";
  }

  return out << Quote(*q.value);
}

ostream& operator<<(ostream& out, Quoter<ShortcutDetails> q) {
  out << "{" << PinManager::Id(q.value.target_stable_id);

  if (q.value.target_lookup_status != LookupStatus::kOk) {
    out << " " << Quote(q.value.target_lookup_status);
  }

  return out << "}";
}

ostream& operator<<(ostream& out, Quoter<FileMetadata> q) {
  const FileMetadata& md = q.value;

  out << "{" << Quote(md.type) << " " << PinManager::Id(md.stable_id);

  if (md.size != 0) {
    out << " of " << HumanReadableSize(md.size);
  }

  if (md.trashed) {
    out << ", trashed";
  }

  if (md.can_pin != FileMetadata::CanPinStatus::kOk) {
    out << ", not pinnable";
  }

  if (VLOG_IS_ON(2)) {
    out << ", pinned: " << md.pinned;
  } else if (md.pinned) {
    out << ", pinned";
  }

  if (VLOG_IS_ON(2)) {
    out << ", available_offline: " << md.available_offline;
  } else if (md.available_offline) {
    out << ", available offline";
  }

  if (md.shortcut_details) {
    out << ", shortcut to " << Quote(*md.shortcut_details);
  }

  return out << "}";
}

ostream& operator<<(ostream& out, Quoter<mojom::ItemEvent> q) {
  const mojom::ItemEvent& e = q.value;
  return out << "{" << Quote(e.state) << " " << PinManager::Id(e.stable_id)
             << " " << Quote(e.path) << ", bytes_transferred: "
             << HumanReadableSize(e.bytes_transferred)
             << ", bytes_to_transfer: "
             << HumanReadableSize(e.bytes_to_transfer)
             << ", is_download: " << e.is_download << "}";
}

ostream& operator<<(ostream& out, Quoter<mojom::FileChange> q) {
  const mojom::FileChange& change = q.value;
  return out << "{" << Quote(change.type) << " "
             << PinManager::Id(change.stable_id) << " " << Quote(change.path)
             << "}";
}

ostream& operator<<(ostream& out, Quoter<mojom::DriveError> q) {
  const mojom::DriveError& e = q.value;
  return out << "{" << Quote(e.type) << " " << PinManager::Id(e.stable_id)
             << " " << Quote(e.path) << "}";
}

// Rounds the given size to the next multiple of 4-KB.
int64_t RoundToBlockSize(int64_t size) {
  const int64_t block_size = 4 << 10;  // 4 KB
  const int64_t mask = block_size - 1;
  static_assert((block_size & mask) == 0, "block_size must be a power of 2");
  return (size + mask) & ~mask;
}

int64_t GetSize(const FileMetadata& metadata) {
  const int64_t kAverageHostedFileSize = 7800;
  return metadata.type == FileMetadata::Type::kHosted ? kAverageHostedFileSize
                                                      : metadata.size;
}

}  // namespace

std::ostream& NiceNum(std::ostream& out) {
  out.imbue(NiceNumLocale());
  return out;
}

ostream& operator<<(ostream& out, const PinManager::Id id) {
  return out << "#" << static_cast<int64_t>(id);
}

ostream& operator<<(ostream& out, HumanReadableSize size) {
  int64_t i = static_cast<int64_t>(size);
  if (i == 0) {
    return out << "zilch";
  }

  if (i < 0) {
    out << '-';
    i = -i;
  }

  {
    std::locale old_locale = out.imbue(NiceNumLocale());
    out << i << " bytes";
    out.imbue(std::move(old_locale));
  }

  if (i < 1024) {
    return out;
  }

  double d = static_cast<double>(i) / 1024;
  const char* unit = "KMGT";
  while (d >= 1024 && *unit != '\0') {
    d /= 1024;
    unit++;
  }

  return out << " (" << std::setprecision(4) << d << " " << *unit << ")";
}

ostream& PinManager::File::PrintTo(ostream& out) const {
  return out << "{path: " << Quote(path)
             << ", transferred: " << HumanReadableSize(transferred)
             << ", total: " << HumanReadableSize(total)
             << ", pinned: " << pinned << ", in_progress: " << in_progress
             << "}";
}

Progress::Progress() = default;
Progress::Progress(const Progress&) = default;
Progress& Progress::operator=(const Progress&) = default;

bool Progress::HasEnoughFreeSpace() const {
  // The free space should not go below this limit.
  const int64_t margin = cryptohome::kMinFreeSpaceInBytes;
  const bool enough = required_space + margin <= free_space;
  LOG_IF(ERROR, !enough) << "Not enough space: Free space "
                         << HumanReadableSize(free_space)
                         << " is less than required space "
                         << HumanReadableSize(required_space) << " + margin "
                         << HumanReadableSize(margin);
  return enough;
}

bool Progress::IsError() const {
  switch (stage) {
    case Stage::kCannotGetFreeSpace:
    case Stage::kCannotListFiles:
    case Stage::kNotEnoughSpace:
    case Stage::kCannotEnableDocsOffline:
      return true;

    case Stage::kGettingFreeSpace:
    case Stage::kListingFiles:
    case Stage::kPaused:
    case Stage::kSuccess:
    case Stage::kSyncing:
    case Stage::kStopped:
      return false;
  }

  NOTREACHED_NORETURN() << "Unexpected Stage " << Quote(stage);
}

bool InProgress(const Stage stage) {
  switch (stage) {
    case Stage::kGettingFreeSpace:
    case Stage::kListingFiles:
    case Stage::kSyncing:
      return true;

    case Stage::kStopped:
    case Stage::kPaused:
    case Stage::kSuccess:
    case Stage::kCannotGetFreeSpace:
    case Stage::kCannotListFiles:
    case Stage::kNotEnoughSpace:
    case Stage::kCannotEnableDocsOffline:
      return false;
  }

  NOTREACHED_NORETURN() << "Unexpected Stage " << Quote(stage);
}

std::string ToString(Stage stage) {
  return (std::ostringstream() << Quote(stage)).str();
}

std::string ToString(TimeDelta time_delta) {
  return (std::ostringstream() << Quote(time_delta)).str();
}

constexpr TimeDelta kStalledFileInterval = base::Seconds(10);

bool PinManager::CanPin(const FileMetadata& md, const Path& path) {
  using Type = FileMetadata::Type;
  const auto id = PinManager::Id(md.stable_id);

  if (md.shortcut_details) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Shortcut to "
            << Id(md.shortcut_details->target_stable_id);
    return false;
  }

  if (md.type == Type::kDirectory) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Directory";
    return false;
  }

  if (md.can_pin != FileMetadata::CanPinStatus::kOk) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Cannot be pinned";
    return false;
  }

  if (md.pinned && md.available_offline) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Already pinned";
    return false;
  }

  if (md.trashed) {
    VLOG(1) << "Skipped " << id << " " << Quote(path) << ": Trashed";
    return false;
  }

  return true;
}

bool PinManager::Add(const FileMetadata& md, const Path& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Id id = Id(md.stable_id);
  VLOG(3) << "Considering " << id << " " << Quote(path) << " " << Quote(md);

  if (!CanPin(md, path)) {
    progress_.skipped_items++;
    return false;
  }

  const int64_t size = GetSize(md);
  DCHECK_GE(size, 0) << " for " << id << " " << Quote(path);

  const auto [it, ok] =
      files_to_track_.try_emplace(id, File{.path = path,
                                           .total = size,
                                           .pinned = md.pinned,
                                           .in_progress = true});
  DCHECK_EQ(id, it->first);
  File& file = it->second;
  if (!ok) {
    LOG_IF(ERROR, !ok) << "Cannot add " << id << " " << Quote(path)
                       << " with size " << HumanReadableSize(size)
                       << " to the files to track: Conflicting entry " << file;
    return false;
  }

  VLOG(3) << "Added " << id << " " << Quote(path) << " with size "
          << HumanReadableSize(size) << " to the files to track";

  progress_.files_to_pin++;
  progress_.bytes_to_pin += size;

  if (md.pinned) {
    progress_.syncing_files++;
    DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  } else {
    files_to_pin_.insert(id);
    DCHECK_LE(files_to_pin_.size(),
              static_cast<size_t>(progress_.files_to_pin));
  }

  if (md.available_offline) {
    file.transferred = size;
    progress_.pinned_bytes += size;
  } else {
    DCHECK_EQ(file.transferred, 0);
    progress_.required_space += RoundToBlockSize(size);
  }

  VLOG_IF(1, md.pinned && !md.available_offline)
      << "Already pinned but not available offline yet: " << id << " "
      << Quote(path);
  VLOG_IF(1, !md.pinned && md.available_offline)
      << "Not pinned yet but already available offline: " << id << " "
      << Quote(path);

  return true;
}

bool PinManager::Remove(const Id id,
                        const Path& path,
                        const int64_t transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(3) << "Not tracked: " << id << " " << path;
    return false;
  }

  DCHECK_EQ(it->first, id);
  Remove(it, path, transferred);
  return true;
}

void PinManager::Remove(const Files::iterator it,
                        const Path& path,
                        const int64_t transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(it != files_to_track_.end());
  const Id id = it->first;

  {
    const File& file = it->second;

    if (transferred < 0) {
      Update(*it, path, file.total, -1);
    } else {
      Update(*it, path, transferred, transferred);
    }

    if (file.pinned) {
      progress_.syncing_files--;
      DCHECK_EQ(files_to_pin_.count(id), 0u);
    } else {
      const size_t erased = files_to_pin_.erase(id);
      DCHECK_EQ(erased, 1u);
    }
  }

  files_to_track_.erase(it);
  DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  VLOG(3) << "Stopped tracking " << id << " " << Quote(path);
}

bool PinManager::Update(const Id id,
                        const Path& path,
                        const int64_t transferred,
                        const int64_t total) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(3) << "Not tracked: " << id << " " << path;
    return false;
  }

  DCHECK_EQ(it->first, id);
  return Update(*it, path, transferred, total);
}

bool PinManager::Update(Files::value_type& entry,
                        const Path& path,
                        const int64_t transferred,
                        const int64_t total) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Id id = entry.first;
  File& file = entry.second;
  bool modified = false;

  if (path != file.path) {
    VLOG(1) << "Changed path of " << id << " from " << Quote(file.path)
            << " to " << Quote(path);
    file.path = path;
    modified = true;
  }

  if (transferred != file.transferred && transferred >= 0) {
    LOG_IF(ERROR, transferred < file.transferred)
        << "Progress went backwards from "
        << HumanReadableSize(file.transferred) << " to "
        << HumanReadableSize(transferred) << " for " << id << " "
        << Quote(path);
    progress_.pinned_bytes += transferred - file.transferred;
    progress_.required_space -=
        RoundToBlockSize(transferred) - RoundToBlockSize(file.transferred);
    file.transferred = transferred;
    modified = true;
  }

  if (total != file.total && total >= 0) {
    LOG(ERROR) << "Changed expected size of " << id << " " << Quote(path)
               << " from " << HumanReadableSize(file.total) << " to "
               << HumanReadableSize(total);
    progress_.bytes_to_pin += total - file.total;
    progress_.required_space +=
        RoundToBlockSize(total) - RoundToBlockSize(file.total);
    file.total = total;
    modified = true;
  }

  if (modified) {
    file.in_progress = true;
  }

  return modified;
}

PinManager::PinManager(Path profile_path, mojom::DriveFs* const drivefs)
    : profile_path_(std::move(profile_path)),
      drivefs_(drivefs),
      space_getter_(base::BindRepeating(&GetFreeSpace)) {
  DCHECK(drivefs_);
}

PinManager::~PinManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(progress_.stage))
      << "Pin manager is " << Quote(progress_.stage);

  for (Observer& observer : observers_) {
    observer.OnDrop();
  }
  observers_.Clear();
}

void PinManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (InProgress(progress_.stage)) {
    LOG(ERROR) << "Pin manager is already started: " << Quote(progress_.stage);
    return;
  }

  VLOG(1) << "Starting";
  progress_ = {};
  listed_items_.clear();
  files_to_pin_.clear();
  files_to_track_.clear();
  DCHECK_EQ(progress_.syncing_files, 0);

  if (!is_online_) {
    LOG(WARNING) << "Device is currently offline";
    return Complete(Stage::kPaused);
  }

  VLOG(2) << "Getting free space";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kGettingFreeSpace;
  NotifyProgress();

  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&PinManager::OnFreeSpaceRetrieved1, GetWeakPtr()));
}

void PinManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!progress_.IsError()) {
    VLOG(1) << "Stopping";
    Complete(Stage::kStopped);
  }
}

void PinManager::CalculateRequiredSpace() {
  ShouldPin(false);
  Start();
}

void PinManager::OnFreeSpaceRetrieved1(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kGettingFreeSpace);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot get free space: " << free_space;
    return Complete(Stage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Free space: " << HumanReadableSize(free_space);
  VLOG(1) << "Listing files";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kListingFiles;
  NotifyProgress();

  DCHECK_EQ(progress_.total_queries, 0);
  DCHECK_EQ(progress_.active_queries, 0);
  DCHECK_EQ(progress_.max_active_queries, 0);
  ListItems(Id::kNone, Path("/root"));
}

void PinManager::CheckFreeSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(2) << "Getting free space";
  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&PinManager::OnFreeSpaceRetrieved2, GetWeakPtr()));
}

void PinManager::OnFreeSpaceRetrieved2(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot get free space: " << free_space;
    return Complete(Stage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Free space: " << HumanReadableSize(progress_.free_space);
  NotifyProgress();

  if (!progress_.HasEnoughFreeSpace()) {
    return Complete(Stage::kNotEnoughSpace);
  }

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PinManager::CheckFreeSpace, GetWeakPtr()),
      space_check_interval_);
}

void PinManager::ListItems(const Id dir_id, Path dir_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Visiting " << dir_id << " " << Quote(dir_path);

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->page_size = 1000;
  params->my_drive_results_only = true;
  params->parent_stable_id = static_cast<int64_t>(dir_id);

  Query query;
  drivefs_->StartSearchQuery(query.BindNewPipeAndPassReceiver(),
                             std::move(params));

  progress_.total_queries++;
  progress_.active_queries++;
  VLOG(2) << NiceNum << "Active queries: " << progress_.active_queries;

  if (progress_.max_active_queries < progress_.active_queries) {
    progress_.max_active_queries = progress_.active_queries;
  }

  DCHECK_GE(progress_.total_queries, progress_.active_queries);

  GetNextPage(dir_id, std::move(dir_path), std::move(query));
}

void PinManager::GetNextPage(const Id dir_id, Path dir_path, Query query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kListingFiles);
  DCHECK(query);
  // Get the underlying pointer because we going to move `query`.
  mojom::SearchQuery* const q = query.get();
  VLOG(2) << "Getting next batch of items from " << dir_id << " "
          << Quote(dir_path);
  DCHECK(q);
  q->GetNextPage(base::BindOnce(
      [](const base::WeakPtr<PinManager> pin_manager, Id dir_id, Path dir_path,
         Query query, const drive::FileError error,
         const absl::optional<std::vector<QueryItemPtr>> items) {
        if (pin_manager) {
          pin_manager->OnSearchResult(
              dir_id, std::move(dir_path), std::move(query), error,
              items ? *items : base::span<const QueryItemPtr>{});
        } else {
          VLOG(1) << "Dropped query for " << dir_id << " " << Quote(dir_path);
        }
      },
      GetWeakPtr(), dir_id, std::move(dir_path), std::move(query)));
}

void PinManager::OnSearchResult(const Id dir_id,
                                Path dir_path,
                                Query query,
                                const drive::FileError error,
                                const base::span<const QueryItemPtr> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kListingFiles);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot visit " << dir_id << " " << Quote(dir_path) << ": "
               << error;
    switch (error) {
      default:
        return Complete(Stage::kCannotListFiles);

      case drive::FILE_ERROR_NO_CONNECTION:
      case drive::FILE_ERROR_SERVICE_UNAVAILABLE:
        const TimeDelta delay = base::Seconds(5);
        LOG(ERROR) << "Will retry in " << Quote(delay);
        SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&PinManager::GetNextPage, GetWeakPtr(), dir_id,
                           std::move(dir_path), std::move(query)),
            delay);
        return;
    }
  }

  progress_.time_spent_listing_items = timer_.Elapsed();

  if (items.empty()) {
    VLOG(1) << "Visited " << dir_id << " " << Quote(dir_path);

    DCHECK_LE(progress_.active_queries, progress_.max_active_queries);
    DCHECK_GT(progress_.active_queries, 0);
    if (--progress_.active_queries != 0) {
      VLOG(2) << NiceNum << "Active queries: " << progress_.active_queries;
      return;
    }

    VLOG(1) << "Finished listing files in "
            << Quote(progress_.time_spent_listing_items);
    VLOG(1) << NiceNum << "Total queries: " << progress_.total_queries;
    VLOG(1) << NiceNum
            << "Max active queries: " << progress_.max_active_queries;
    VLOG(1) << NiceNum << "Found " << progress_.listed_items
            << " items: " << progress_.listed_dirs << " dirs, "
            << progress_.listed_files << " files, " << progress_.listed_docs
            << " docs, " << progress_.listed_shortcuts << " shortcuts";
    VLOG(1) << NiceNum << "Tracking " << files_to_track_.size() << " files";
    listed_items_.clear();
    return StartPinning();
  }

  progress_.listed_items += items.size();
  VLOG(2) << "Got " << items.size() << " items from " << dir_id << " "
          << Quote(dir_path);

  for (const QueryItemPtr& item : items) {
    DCHECK(item);
    HandleQueryItem(dir_id, dir_path, *item);
  }

  VLOG(1) << NiceNum << "Listed " << progress_.listed_items << " items in "
          << Quote(progress_.time_spent_listing_items) << ", Skipped "
          << progress_.skipped_items << " items, Tracking "
          << files_to_track_.size() << " files";
  NotifyProgress();
  GetNextPage(dir_id, std::move(dir_path), std::move(query));
}

void PinManager::HandleQueryItem(Id dir_id,
                                 const Path& dir_path,
                                 const QueryItem& item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(item.metadata);
  FileMetadata& md = *item.metadata;
  Id id = Id(md.stable_id);
  const Path& path = item.path;

  VLOG(2) << "Listed " << id << " " << Quote(path) << ": " << Quote(md);

  if (!dir_path.IsParent(path)) {
    // This can happen when the parent folder was found by following a shortcut.
    VLOG(2) << Quote(md.type) << " " << id << " " << Quote(path)
            << " is not in Directory " << dir_id << " " << Quote(dir_path);
  }

  // Is this item a shortcut?
  if (md.shortcut_details) {
    progress_.listed_shortcuts++;

    // Is the shortcut's target accessible?
    if (md.shortcut_details->target_lookup_status != LookupStatus::kOk) {
      // The shortcut target is not accessible.
      progress_.skipped_items++;
      VLOG(1) << "Broken shortcut " << id << " " << Quote(path) << ": "
              << "Target " << Quote(md.type) << " "
              << Id(md.shortcut_details->target_stable_id)
              << " has lookup error "
              << Quote(md.shortcut_details->target_lookup_status) << ": "
              << Quote(md);
      return;
    }

    // Is the shortcut's target in the trash bin?
    if (md.trashed) {
      // The shortcut target is in the trash bin.
      progress_.skipped_items++;
      VLOG(1) << "Broken shortcut " << id << " " << Quote(path) << ": "
              << "Target " << Quote(md.type) << " "
              << Id(md.shortcut_details->target_stable_id)
              << " is trashed: " << Quote(md);
      return;
    }

    // The shortcut target is accessible.
    VLOG(1) << "Following shortcut " << id << " " << Quote(path) << " to "
            << Quote(md.type) << " "
            << Id(md.shortcut_details->target_stable_id) << ": " << Quote(md);

    // Follow the shortcut.
    md.stable_id = md.shortcut_details->target_stable_id;
    id = Id(md.stable_id);
    md.shortcut_details.reset();
  }

  // Deduplicate items.
  if (const auto [it, ok] = listed_items_.try_emplace(id, dir_id); !ok) {
    DCHECK_EQ(it->first, id);
    progress_.skipped_items++;
    VLOG(1) << "Skipped " << Quote(md.type) << " " << id << " " << Quote(path)
            << " " << Quote(md) << " seen in Directory " << dir_id << " "
            << Quote(dir_path) << ": Previously seen in Directory "
            << it->second;
    return;
  }

  using Type = FileMetadata::Type;
  switch (md.type) {
    case Type::kFile:
      progress_.listed_files++;
      Add(md, path);
      return;

    case Type::kHosted:
      progress_.listed_docs++;
      Add(md, path);
      return;

    case Type::kDirectory:
      progress_.listed_dirs++;
      ListItems(id, path);
      return;
  }

  progress_.skipped_items++;
  LOG(ERROR) << "Unexpected item type " << Quote(md.type) << " for " << id
             << " " << path << ": " << Quote(md);
}

void PinManager::Complete(const Stage stage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(stage));

  progress_.stage = stage;
  switch (stage) {
    case Stage::kSuccess:
      VLOG(1) << "Finished with success";
      break;

    case Stage::kPaused:
      VLOG(1) << "Paused";
      break;

    case Stage::kStopped:
      VLOG(1) << "Stopped";
      break;

    default:
      LOG(ERROR) << "Finished with error: " << Quote(stage);
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  listed_items_.clear();
  files_to_pin_.clear();
  files_to_track_.clear();
  progress_.syncing_files = 0;
  progress_.active_queries = 0;
  NotifyProgress();

  if (completion_callback_) {
    std::move(completion_callback_).Run(stage);
  }
}

void PinManager::StartPinning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kListingFiles);

  VLOG(1) << NiceNum << "To pin: " << files_to_pin_.size() << " files, "
          << HumanReadableSize(progress_.bytes_to_pin);
  VLOG(1) << "Required space: " << HumanReadableSize(progress_.required_space);

  if (!progress_.HasEnoughFreeSpace()) {
    return Complete(Stage::kNotEnoughSpace);
  }

  if (!should_pin_) {
    VLOG(1) << "Should not pin files";
    return Complete(Stage::kSuccess);
  }

  VLOG(1) << "Enabling Docs offline";
  drivefs_->SetDocsOfflineEnabled(
      true, base::BindOnce(&PinManager::OnDocsOfflineEnabled, GetWeakPtr()));
}

void PinManager::OnDocsOfflineEnabled(drive::FileError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot enable Docs offline: " << error;
    return Complete(Stage::kCannotEnableDocsOffline);
  }

  VLOG(1) << "Successfully enabled Docs offline";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kSyncing;
  NotifyProgress();

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PinManager::CheckStalledFiles, GetWeakPtr()),
      kStalledFileInterval);

  CheckFreeSpace();
  PinSomeFiles();
}

void PinManager::PinSomeFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != Stage::kSyncing) {
    return NotifyProgress();
  }

  while (progress_.syncing_files < 50 && !files_to_pin_.empty()) {
    const Id id = files_to_pin_.extract(files_to_pin_.begin()).value();
    const Files::iterator it = files_to_track_.find(id);
    DCHECK(it != files_to_track_.end()) << "Not tracked: " << id;
    DCHECK_EQ(it->first, id);
    File& file = it->second;
    const Path& path = file.path;

    DCHECK(!file.pinned) << "Already pinned: " << id << " " << Quote(path);
    VLOG(2) << "Pinning " << id << " " << Quote(path);
    drivefs_->SetPinnedByStableId(
        static_cast<int64_t>(id), true,
        base::BindOnce(&PinManager::OnFilePinned, GetWeakPtr(), id, path));

    file.pinned = true;
    progress_.syncing_files++;
    DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  }

  if (!progress_.emptied_queue) {
    progress_.time_spent_pinning_files = timer_.Elapsed();
  }

  bool notify_progress = false;
  if (progress_timer_.Elapsed() >= base::Seconds(1)) {
    notify_progress = true;
    VLOG(1) << NiceNum << "Progress "
            << Percentage(progress_.pinned_bytes, progress_.bytes_to_pin)
            << "%: Synced " << HumanReadableSize(progress_.pinned_bytes)
            << " and " << progress_.pinned_files << " files in "
            << Quote(progress_.time_spent_pinning_files) << ", Syncing "
            << progress_.syncing_files << " files";
    progress_timer_ = {};
  }

  if (files_to_track_.empty() && !progress_.emptied_queue) {
    notify_progress = true;
    progress_.emptied_queue = true;
    LOG_IF(ERROR, progress_.failed_files > 0)
        << NiceNum << "Failed to pin " << progress_.failed_files << " files";
    VLOG(1) << NiceNum << "Pinned " << progress_.pinned_files << " files and "
            << HumanReadableSize(progress_.pinned_bytes) << " in "
            << Quote(progress_.time_spent_pinning_files);
    VLOG(2) << NiceNum << "Useful events: " << progress_.useful_events;
    VLOG(2) << NiceNum << "Duplicated events: " << progress_.duplicated_events;
  }

  if (notify_progress) {
    NotifyProgress();
  }
}

void PinManager::OnFilePinned(const Id id,
                              const Path& path,
                              const drive::FileError status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << id << " " << Quote(path) << ": " << status;
    if (Remove(id, path, 0)) {
      progress_.failed_files++;
      PinSomeFiles();
    }
    return;
  }

  VLOG(1) << "Pinned " << id << " " << Quote(path);

  const auto it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    LOG(ERROR) << "Got unexpected notification that " << id << " "
               << Quote(path) << " was pinned: The item is not tracked";
    DCHECK(!files_to_pin_.contains(id));
    return;
  }

  DCHECK_EQ(it->first, id);
  File& file = it->second;
  if (!file.pinned) {
    LOG(ERROR)
        << "Got unexpected notification that " << id << " " << Quote(path)
        << " was pinned: The item is not remembered as having been pinned";
    file.pinned = true;
    const size_t erased = files_to_pin_.erase(id);
    DCHECK_EQ(erased, 1u);
    return;
  }

  DCHECK(!files_to_pin_.contains(id));
}

void PinManager::OnSyncingStatusUpdate(const mojom::SyncingStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const mojom::ItemEventPtr& event : status.item_events) {
    DCHECK(event);

    if (!InProgress(progress_.stage)) {
      VLOG(2) << "Ignored " << Quote(*event);
      continue;
    }

    if (OnSyncingEvent(*event)) {
      progress_.useful_events++;
    } else {
      progress_.duplicated_events++;
      VLOG(3) << "Discarded " << Quote(*event);
    }
  }

  PinSomeFiles();
}

bool PinManager::OnSyncingEvent(mojom::ItemEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!event.is_download) {
    // We're only interested in download events.
    VLOG(3) << "Ignored upload-related event " << Quote(event);
    return false;
  }

  const Id id = Id(event.stable_id);
  const Path path = Path(event.path);

  using State = mojom::ItemEvent::State;
  switch (event.state) {
    case State::kQueued:
    case State::kInProgress:
      if (!Update(id, path, event.bytes_transferred, event.bytes_to_transfer)) {
        return false;
      }

      VLOG(3) << Quote(event.state) << " " << id << " " << Quote(path) << ": "
              << Quote(event);
      VLOG_IF(2, !VLOG_IS_ON(3))
          << Quote(event.state) << " " << id << " " << Quote(path);
      return true;

    case State::kCompleted:
      if (!Remove(id, path)) {
        return false;
      }

      VLOG(2) << "Synced " << id << " " << Quote(path) << ": " << Quote(event);
      VLOG_IF(1, !VLOG_IS_ON(2)) << "Synced " << id << " " << Quote(path);
      progress_.pinned_files++;
      return true;

    case State::kFailed:
      if (!Remove(id, path, 0)) {
        return false;
      }

      LOG(ERROR) << Quote(event.state) << " " << id << " " << Quote(path)
                 << ": " << Quote(event);
      progress_.failed_files++;
      return true;
  }

  LOG(ERROR) << "Unexpected event type: " << Quote(event);
  return false;
}

void PinManager::NotifyDelete(const Id id, const Path& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!Remove(id, path, 0)) {
    VLOG(1) << "Not tracked: " << id << " " << Quote(path);
    return;
  }

  VLOG(1) << "Stopped tracking " << id << " " << Quote(path);
  progress_.failed_files++;
  PinSomeFiles();
}

void PinManager::OnUnmounted() {
  VLOG(1) << "Unmounted DriveFS";
}

void PinManager::OnFilesChanged(const std::vector<mojom::FileChange>& changes) {
  using Type = mojom::FileChange::Type;
  for (const mojom::FileChange& event : changes) {
    switch (event.type) {
      case Type::kCreate:
        OnFileCreated(event);
        continue;

      case Type::kDelete:
        OnFileDeleted(event);
        continue;

      case Type::kModify:
        OnFileModified(event);
        continue;
    }

    VLOG(1) << "Unexpected FileChange type " << Quote(event);
  }
}

void PinManager::OnFileCreated(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kCreate);

  if (!InProgress(progress_.stage)) {
    VLOG(2) << "Ignored " << Quote(event) << ": PinManager is currently "
            << Quote(progress_.stage);
    return;
  }

  const Id id = Id(event.stable_id);
  const Path& path = event.path;

  if (id == Id::kNone) {
    // Ignore spurious event (b/268419828).
    VLOG(2) << "Ignored " << Quote(event) << ": Spurious event";
    return;
  }

  if (!Path("/root").IsParent(path)) {
    VLOG(2) << "Ignored " << Quote(event) << ": Not in 'My Drive'";
    return;
  }

  if (const Files::iterator it = files_to_track_.find(id);
      it != files_to_track_.end()) {
    DCHECK_EQ(it->first, id);
    LOG(ERROR) << "Ignored " << Quote(event) << ": Existing entry "
               << it->second;
    return;
  }

  VLOG(1) << "Got " << Quote(event);
  drivefs_->GetMetadataByStableId(
      static_cast<int64_t>(id),
      base::BindOnce(&PinManager::OnMetadataForCreatedFile, GetWeakPtr(), id,
                     path));
}

void PinManager::OnFileDeleted(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kDelete);

  VLOG(1) << "Got " << Quote(event);
  NotifyDelete(Id(event.stable_id), event.path);
}

void PinManager::OnFileModified(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kModify);

  const Id id = Id(event.stable_id);
  const Path& path = event.path;

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(1) << "Ignored " << Quote(event) << ": Not tracked";
    return;
  }

  VLOG(1) << "Got " << Quote(event);
  DCHECK_EQ(it->first, id);

  Update(*it, path, -1, -1);

  VLOG(2) << "Checking changed " << id << " " << Quote(path);
  drivefs_->GetMetadataByStableId(
      static_cast<int64_t>(id),
      base::BindOnce(&PinManager::OnMetadataForModifiedFile, GetWeakPtr(), id,
                     path));
}

void PinManager::OnError(const mojom::DriveError& error) {
  LOG(ERROR) << "Got DriveError " << Quote(error);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error.type == mojom::DriveError::Type::kPinningFailedDiskFull &&
      InProgress(progress_.stage)) {
    Complete(Stage::kNotEnoughSpace);
  }
}

void PinManager::NotifyProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnProgress(progress_);
  }
}

void PinManager::CheckStalledFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& [id, file] : files_to_track_) {
    if (!file.pinned) {
      DCHECK(files_to_pin_.contains(id));
      continue;
    }

    if (file.in_progress) {
      file.in_progress = false;
      continue;
    }

    const Path& path = file.path;
    VLOG(1) << "Checking stalled " << id << " " << Quote(path);
    drivefs_->GetMetadataByStableId(
        static_cast<int64_t>(id),
        base::BindOnce(&PinManager::OnMetadataForModifiedFile, GetWeakPtr(), id,
                       path));
  }

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PinManager::CheckStalledFiles, GetWeakPtr()),
      kStalledFileInterval);
}

void PinManager::OnMetadataForCreatedFile(const Id id,
                                          const Path& path,
                                          const drive::FileError error,
                                          const FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of created " << id << " " << Quote(path)
               << ": " << error;
    return NotifyDelete(id, path);
  }

  DCHECK(metadata);
  const FileMetadata& md = *metadata;
  DCHECK_EQ(id, Id(md.stable_id));
  VLOG(2) << "Got metadata of created " << id << " " << Quote(path) << ": "
          << Quote(md);

  if (Add(md, path)) {
    PinSomeFiles();
  }
}

void PinManager::OnMetadataForModifiedFile(const Id id,
                                           const Path& path,
                                           const drive::FileError error,
                                           const FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of modified " << id << " " << Quote(path)
               << ": " << error;
    return NotifyDelete(id, path);
  }

  DCHECK(metadata);
  const FileMetadata& md = *metadata;
  DCHECK_EQ(id, Id(md.stable_id));

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(1) << "Ignored metadata of untracked " << id << " " << Quote(path)
            << ": " << Quote(md);
    return;
  }

  DCHECK_EQ(it->first, id);
  const File& file = it->second;
  VLOG(2) << "Got metadata of modified " << id << " " << Quote(path) << ": "
          << Quote(md);

  if (!md.pinned) {
    if (!file.pinned) {
      VLOG(1) << "Modified " << id << " " << Quote(path)
              << " is still scheduled to be pinned";
      DCHECK(files_to_pin_.contains(id));
      return;
    }

    DCHECK(file.pinned);
    LOG(ERROR) << "Got unexpectedly unpinned: " << id << " " << Quote(path);
    Remove(it, path, 0);
    progress_.failed_files++;
    return PinSomeFiles();
  }

  DCHECK(md.pinned);
  if (md.available_offline) {
    Remove(it, path, GetSize(md));
    VLOG(1) << "Synced " << id << " " << Quote(path);
    progress_.pinned_files++;
    PinSomeFiles();
  }
}

void PinManager::SetOnline(const bool online) {
  VLOG(2) << "Online: " << online;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_online_ = online;

  if (!is_online_ && InProgress(progress_.stage)) {
    VLOG(1) << "Going offline";
    return Complete(Stage::kPaused);
  }

  if (is_online_ && progress_.stage == Stage::kPaused) {
    VLOG(1) << "Coming back online";
    return Start();
  }
}

}  // namespace drivefs::pinning
