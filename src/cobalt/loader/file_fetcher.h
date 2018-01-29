// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_FILE_FETCHER_H_
#define COBALT_LOADER_FILE_FETCHER_H_

#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// FileFetcher is for fetching data from files on local disk. The file path that
// is passed into the constructor shouldn't include the root directory,
// e.g. it could be "cobalt/loader/testdata/...".
class FileFetcher : public Fetcher {
 public:
  struct Options {
   public:
    Options()
        : buffer_size(kDefaultBufferSize),
          start_offset(0),
          bytes_to_read(std::numeric_limits<int64>::max()),
          message_loop_proxy(base::MessageLoopProxy::current()) {}

    int32 buffer_size;
    int64 start_offset;
    int64 bytes_to_read;
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
    FilePath extra_search_dir;
  };

  FileFetcher(const FilePath& file_path, Handler* handler,
              const Options& options);
  ~FileFetcher() override;

  // This function is used for binding callback for creating FileFetcher.
  static scoped_ptr<Fetcher> Create(const FilePath& file_path,
                                    const Options& options, Handler* handler) {
    return scoped_ptr<Fetcher>(new FileFetcher(file_path, handler, options));
  }

 private:
  // Default size of the buffer that FileFetcher will use to load data.
  static const int32 kDefaultBufferSize = 64 * 1024;

  // Builds the search path list, including an optional extra directory.
  void BuildSearchPath(const FilePath& extra_search_dir);

  // Tries opening a file using the current entry in the seach path.
  void TryFileOpen();

  void ReadNextChunk();
  void CloseFile();
  const char* PlatformFileErrorToString(base::PlatformFileError error);

  // Callbacks for FileUtilProxy functions.
  void DidCreateOrOpen(base::PlatformFileError error,
                       base::PassPlatformFile file, bool created);
  void DidRead(base::PlatformFileError error, const char* data,
               int num_bytes_read);

  // Thread checker ensures all calls to the FileFetcher are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;
  // Size of the buffer that FileFetcher will use to load data.
  int32 buffer_size_;
  // Handle of the input file.
  base::PlatformFile file_;
  // Current offset in the input file.
  int64 file_offset_;
  // How many bytes we are going to read.
  int64 bytes_left_to_read_;
  // Message loop that is used for actual IO operations in FileUtilProxy.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  // Relative (to the current search path entry) file path.
  FilePath file_path_;
  // Paths to search for files. When a file is to be fetched, the fetcher will
  // look at each path in here until the file is found or the end is reached.
  std::vector<FilePath> search_path_;
  // Used internally to support callbacks with weak references to self.
  base::WeakPtrFactory<FileFetcher> weak_ptr_factory_;
  // Current iterator into the search path vector.
  std::vector<FilePath>::const_iterator curr_search_path_iter_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FILE_FETCHER_H_
