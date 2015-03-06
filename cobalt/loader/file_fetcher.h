/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOADER_FILE_FETCHER_H_
#define LOADER_FILE_FETCHER_H_

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
// is passed into the constructor shouldn't include DIR_SOURCE_ROOT, e.g. it
// could be "cobalt/loader/testdata/...".
class FileFetcher : public Fetcher {
 public:
  struct Options {
   public:
    Options()
        : buffer_size(kDefaultBufferSize),
          io_message_loop(base::MessageLoopProxy::current()) {}

    int buffer_size;
    scoped_refptr<base::MessageLoopProxy> io_message_loop;
  };

  FileFetcher(const FilePath& file_path, Handler* handler,
              const Options& options);
  ~FileFetcher() OVERRIDE;

  // This function is used for binding callback for creating FileFetcher.
  static scoped_ptr<Fetcher> Create(const FilePath& file_path,
                                    const Options& options, Handler* handler) {
    return scoped_ptr<Fetcher>(new FileFetcher(file_path, handler, options));
  }

 private:
  // Default size of the buffer that FileFetcher will use to load data.
  static const int kDefaultBufferSize = 64 * 1024;

  void ReadNextChunk();
  void CloseFile();
  const std::string& PlatformFileErrorToString(base::PlatformFileError error);

  // Callbacks for FileUtilProxy functions.
  void DidCreateOrOpen(base::PlatformFileError error,
                       base::PassPlatformFile file, bool created);
  void DidRead(base::PlatformFileError error, const char* data,
               int num_bytes_read);

  // Size of the buffer that FileFetcher will use to load data.
  int buffer_size_;
  // Handle of the input file.
  base::PlatformFile file_;
  // Current offset in the input file.
  int64 file_offset_;
  // Message loop that is used for actual IO operations in FileUtilProxy.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  // Used internally to support callbacks with weak references to self.
  base::WeakPtrFactory<FileFetcher> weak_ptr_factory_;
  // Used to verify that methods are called from the creator thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_FILE_FETCHER_H_
