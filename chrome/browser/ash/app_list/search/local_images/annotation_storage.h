// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace app_list {

class ImageAnnotationWorker;
class SqlDatabase;

// Image metadata retrieved from the database.
struct ImageInfo {
  // All the annotations attributed to the image.
  std::set<std::string> annotations;
  // The full path to the image.
  base::FilePath path;
  // The image's last modified time.
  base::Time last_modified;

  ImageInfo(const std::set<std::string>& annotations,
            const base::FilePath& path,
            const base::Time& last_modified);

  ~ImageInfo();
  ImageInfo(const ImageInfo&);
  ImageInfo& operator=(const ImageInfo&) = delete;
};

// A search result with `relevance` to the supplied query.
struct FileSearchResult {
  // THe full path to the file.
  base::FilePath path;
  // The file's last modified time.
  base::Time last_modified;
  // The file's relevance on the scale from 0-1. It represents how closely a
  // query matches the file's annotation.
  double relevance;

  FileSearchResult(const base::FilePath& path,
                   const base::Time& last_modified,
                   double relevance);

  ~FileSearchResult();
  FileSearchResult(const FileSearchResult&);
  FileSearchResult& operator=(const FileSearchResult&) = delete;
};

// A persistent storage to efficiently store, retrieve and search annotations.
// Creates or opens a database under `path_to_db`. If `annotation_worker` is
// not null, it updates the database on file changes.
class AnnotationStorage {
 public:
  enum class TableColumnName {
    kLabel,
    kImagePath,
    kLastModifiedTime,
  };

  AnnotationStorage(const base::FilePath& path_to_db,
                    const std::string& histogram_tag,
                    int current_version_number,
                    std::unique_ptr<ImageAnnotationWorker> annotation_worker);
  AnnotationStorage(const AnnotationStorage&) = delete;
  AnnotationStorage& operator=(const AnnotationStorage&) = delete;
  ~AnnotationStorage();

  // Initializes the db. Must be called before any other method.
  void Initialize();

  // Adds a new image to the storage.
  void Insert(const ImageInfo& image_info);

  // Removes an image from the storage. It does nothing if the file does not
  void Remove(const base::FilePath& image_path);

  // TODO(b/260646344): Remove after implementing a more efficient search.
  // Returns all the stored annotations.
  std::vector<ImageInfo> GetAllAnnotations();

  // Searches the database for a desired `image_path`.
  std::vector<ImageInfo> FindImagePath(const base::FilePath& image_path);

  // Searches for annotations using FuzzyTokenizedStringMatch with relevance to
  // `query` above a fixed threshold.
  std::vector<FileSearchResult> LinearSearchAnnotations(
      const std::u16string& query);

 private:
  std::unique_ptr<ImageAnnotationWorker> annotation_worker_;
  std::unique_ptr<SqlDatabase> sql_database_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_
