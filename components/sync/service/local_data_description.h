// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
#define COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_

#include <string>
#include <vector>

#include "components/sync/base/data_type.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace syncer {

// Representation of a single item to be displayed in the Batch Upload dialog.
struct LocalDataItemModel {
  // This id corresponds to the data item being represented in the model. It
  // is used to link back to the data accurately when needing to process the
  // result of the dialog.
  // The variant allow for different data types to add their Id types. Multiple
  // data types may have the same Id type.
  // The `DataId` is also used as key of map.
  using DataId = std::variant<
      // CONTACT_INFO
      std::string,
      // PASSWORDS
      std::tuple<std::string,
                 GURL,
                 std::u16string,
                 std::u16string,
                 std::u16string>>;

  // Reprensents the id of the underlying data.
  DataId id;

  // Icon url for the icon of the item model. If empty the the icon will be
  // hidden.
  GURL icon_url;

  // Used as the primary text of the item model.
  std::string title;

  // Used as the secondary text of the item model.
  std::string subtitle;

  LocalDataItemModel();
  ~LocalDataItemModel();
  // Copyable.
  LocalDataItemModel(const LocalDataItemModel&);
  LocalDataItemModel& operator=(const LocalDataItemModel&);
  // Movable.
  LocalDataItemModel(LocalDataItemModel&& other);
  LocalDataItemModel& operator=(LocalDataItemModel&& other);

  friend bool operator==(const LocalDataItemModel&,
                         const LocalDataItemModel&) = default;
};

// TODO(crbug.com/373568992): Merge Desktop and Mobile data under common struct.
struct LocalDataDescription {
  // The following data is currently only used on Desktop platforms in the Batch
  // Upload Dialog.
  //
  // Type that the model list represent.
  syncer::DataType type = syncer::DataType::UNSPECIFIED;
  // List of local data model representations.
  std::vector<LocalDataItemModel> local_data_models;

  // The following data is currently only used on Mobile platforms in the Batch
  // Upload view.
  //
  // Actual count of local items.
  size_t item_count = 0;
  // Contains up to 3 distinct domains corresponding to some of the local items,
  // to be used for a preview.
  std::vector<std::string> domains;
  // Count of distinct domains for preview.
  // Note: This may be different from the count of items(`item_count`), since a
  // user might have, for e.g., multiple bookmarks or passwords for the same
  // domain. It may also be different from domains.size(), that one contains
  // only up to 3 elements.
  size_t domain_count = 0;

  LocalDataDescription();

  // `all_urls` should be the corresponding URL for each local data item, e.g.
  // the URL of each local bookmark. In the resulting object, fields will be as
  // below.
  //   item_count: The size of `all_urls`.
  //   domain_count: The number of unique domains in `all_urls`. For instance
  //                 for {a.com, a.com/foo and b.com}, domain_count will be 2.
  //   domains: The first (up to) 3 domains in alphabetical order.
  explicit LocalDataDescription(const std::vector<GURL>& all_urls);

  LocalDataDescription(const LocalDataDescription&);
  LocalDataDescription& operator=(const LocalDataDescription&);
  LocalDataDescription(LocalDataDescription&&);
  LocalDataDescription& operator=(LocalDataDescription&&);

  ~LocalDataDescription();

  friend bool operator==(const LocalDataDescription&,
                         const LocalDataDescription&) = default;
};

// Returns a string that summarizes the domain content of `description`, meant
// to be consumed by the UI. Must not be called if the `description.domains` is
// empty.
std::u16string GetDomainsDisplayText(const LocalDataDescription& description);

// gmock printer helper.
void PrintTo(const LocalDataDescription& local_data_description,
             std::ostream* os);

#if BUILDFLAG(IS_ANDROID)
// Constructs a Java LocalDataDescription from the provided C++
// LocalDataDescription
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaLocalDataDescription(
    JNIEnv* env,
    const syncer::LocalDataDescription& local_data_description);
#endif

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
