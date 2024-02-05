#ifndef COBALT_NETWORK_CUSTOM_URL_FETCHER_RESPONSE_WRITER_H_
#define COBALT_NETWORK_CUSTOM_URL_FETCHER_RESPONSE_WRITER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"

namespace net {

class IOBuffer;
class URLFetcherFileWriter;
class URLFetcherStringWriter;
#if defined(STARBOARD)
class URLFetcherLargeStringWriter;
#endif

class URLFetcherResponseWriter {
 public:
  virtual ~URLFetcherResponseWriter() {}

  // Initializes this instance. If ERR_IO_PENDING is returned, |callback| will
  // be run later with the result. Calling this method again after a
  // Initialize() success results in discarding already written data.
  virtual int Initialize(CompletionOnceCallback callback) = 0;

#if defined(STARBOARD)
  // The user of this class *may* call this function before any calls to Write()
  // to prime the instance with response size, so it has a chance to do some
  // preparation work, like pre-allocate the buffer.
  virtual void OnResponseStarted(int64_t content_length) = 0;
#endif  // defined(STARBOARD)

  // Writes |num_bytes| bytes in |buffer|, and returns the number of bytes
  // written or an error code. If ERR_IO_PENDING is returned, |callback| will be
  // run later with the result.
  virtual int Write(IOBuffer* buffer, int num_bytes,
                    CompletionOnceCallback callback) = 0;

  // Finishes writing. If |net_error| is not OK, this method can be called
  // in the middle of another operation (eg. Initialize() and Write()). On
  // errors (|net_error| not OK), this method may be called before the previous
  // operation completed. In this case, URLFetcherResponseWriter may skip
  // graceful shutdown and completion of the pending operation. After such a
  // failure, the URLFetcherResponseWriter may be reused. If ERR_IO_PENDING is
  // returned, |callback| will be run later with the result.
  virtual int Finish(int net_error, CompletionOnceCallback callback) = 0;

  // Returns this instance's pointer as URLFetcherStringWriter when possible.
  virtual URLFetcherStringWriter* AsStringWriter() { return NULL; }

#if defined(STARBOARD)
  // Returns this instance's pointer as URLFetcherLargeStringWriter when
  // possible.
  virtual URLFetcherLargeStringWriter* AsLargeStringWriter() { return NULL; }
#endif

  // Returns this instance's pointer as URLFetcherFileWriter when possible.
  virtual URLFetcherFileWriter* AsFileWriter() { return NULL; }
};

class URLFetcherStringWriter : public URLFetcherResponseWriter {
 public:
  URLFetcherStringWriter();
  ~URLFetcherStringWriter() override;

  const std::string& data() const { return data_; }

  // URLFetcherResponseWriter overrides:
  int Initialize(CompletionOnceCallback callback) override;
#if defined(STARBOARD)
  void OnResponseStarted(int64_t /*content_length*/) override {}
#endif  // defined(STARBOARD)
  int Write(IOBuffer* buffer, int num_bytes,
            CompletionOnceCallback callback) override;
  int Finish(int net_error, CompletionOnceCallback callback) override;
  URLFetcherStringWriter* AsStringWriter() override;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherStringWriter);
};

#if defined(STARBOARD)
// Memory-conscious URLFetcherResponseWriter implementation for a "large"
// std::string. The string's capacity is preallocated to the size of the content
// and the data can be "moved" out.
// Similar to cobalt::loader::URLFetcherStringWriter but with a different
// preallocation strategy.
class NET_EXPORT URLFetcherLargeStringWriter : public URLFetcherResponseWriter {
 public:
  URLFetcherLargeStringWriter();
  ~URLFetcherLargeStringWriter() override;

  void GetAndResetData(std::string* data);

  // URLFetcherResponseWriter overrides:
  int Initialize(CompletionOnceCallback callback) override;
  void OnResponseStarted(int64_t content_length) override;
  int Write(IOBuffer* buffer, int num_bytes,
            CompletionOnceCallback callback) override;
  int Finish(int net_error, CompletionOnceCallback callback) override;
  URLFetcherLargeStringWriter* AsLargeStringWriter() override;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherLargeStringWriter);
};
#endif

class NET_EXPORT URLFetcherFileWriter : public URLFetcherResponseWriter {
 public:
  // |file_path| is used as the destination path. If |file_path| is empty,
  // Initialize() will create a temporary file.
  URLFetcherFileWriter(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& file_path);
  ~URLFetcherFileWriter() override;

  const base::FilePath& file_path() const { return file_path_; }

  // URLFetcherResponseWriter overrides:
  int Initialize(CompletionOnceCallback callback) override;
#if defined(STARBOARD)
  void OnResponseStarted(int64_t /*content_length*/) override {}
#endif  // defined(STARBOARD)
  int Write(IOBuffer* buffer, int num_bytes,
            CompletionOnceCallback callback) override;
  int Finish(int net_error, CompletionOnceCallback callback) override;
  URLFetcherFileWriter* AsFileWriter() override;

  // Drops ownership of the file at |file_path_|.
  // This class will not delete it or write to it again.
  void DisownFile();

 private:
  // Closes the file if it is open and then delete it.
  void CloseAndDeleteFile();

  // Callback which gets the result of a temporary file creation.
  void DidCreateTempFile(base::FilePath* temp_file_path, bool success);

  // Run |callback_| if it is non-null when FileStream::Open or
  // FileStream::Write is completed.
  void OnIOCompleted(int result);

  // Callback which gets the result of closing a file.
  void CloseComplete(int result);

  // Task runner on which file operations should happen.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Destination file path.
  // Initialize() creates a temporary file if this variable is empty.
  base::FilePath file_path_;

  // True when this instance is responsible to delete the file at |file_path_|.
  bool owns_file_;

  std::unique_ptr<FileStream> file_stream_;

  CompletionOnceCallback callback_;

  base::WeakPtrFactory<URLFetcherFileWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherFileWriter);
};

}  // namespace net


#endif
