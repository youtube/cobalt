// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MHTML_HANDLE_WRITER_H_
#define CONTENT_RENDERER_MHTML_HANDLE_WRITER_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/common/download/mhtml_file_writer.mojom-forward.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace base {
class TaskRunner;
}

namespace blink {
class WebThreadSafeData;
}

namespace mojo {
class DataPipeProducer;
}

namespace content {

// TODO(crbug.com/40606905): This class needs unit tests.

// Handle wrapper for MHTML serialization to abstract the handle which data
// is written to. This is instantiated on the heap and is responsible for
// destroying itself after completing its write operation.
// Should only live in blocking sequenced threads.
class MHTMLHandleWriter {
 public:
  using MHTMLWriteCompleteCallback =
      base::OnceCallback<void(mojom::MhtmlSaveStatus)>;

  MHTMLHandleWriter(scoped_refptr<base::TaskRunner> main_thread_task_runner,
                    MHTMLWriteCompleteCallback callback);

  MHTMLHandleWriter(const MHTMLHandleWriter&) = delete;
  MHTMLHandleWriter& operator=(const MHTMLHandleWriter&) = delete;

  virtual ~MHTMLHandleWriter();

  void WriteContents(std::unique_ptr<MHTMLHandleWriter> self,
                     std::vector<blink::WebThreadSafeData> mhtml_contents);

  // Finalizes the writing operation, recording the UMA, closing the handle,
  // and deleting itself.
  void Finish(std::unique_ptr<MHTMLHandleWriter> self,
              mojom::MhtmlSaveStatus save_status);

 protected:
  virtual void WriteContentsImpl(
      std::unique_ptr<MHTMLHandleWriter> self,
      std::vector<blink::WebThreadSafeData> mhtml_contents) = 0;

  virtual void Close() = 0;

 private:
  scoped_refptr<base::TaskRunner> main_thread_task_runner_;
  MHTMLWriteCompleteCallback callback_;
  bool is_writing_ = false;
};

// Wraps a base::File target to write MHTML contents to.
// This implementation immediately finishes after writing all MHTML contents
// to the file handle.
class MHTMLFileHandleWriter : public MHTMLHandleWriter {
 public:
  MHTMLFileHandleWriter(scoped_refptr<base::TaskRunner> main_thread_task_runner,
                        MHTMLWriteCompleteCallback callback,
                        base::File file);

  MHTMLFileHandleWriter(const MHTMLFileHandleWriter&) = delete;
  MHTMLFileHandleWriter& operator=(const MHTMLFileHandleWriter&) = delete;

  ~MHTMLFileHandleWriter() override;

 protected:
  // Writes the serialized and encoded MHTML data from WebThreadSafeData
  // instances directly to the file handle passed from the Browser.
  void WriteContentsImpl(
      std::unique_ptr<MHTMLHandleWriter> self,
      std::vector<blink::WebThreadSafeData> mhtml_contents) override;

  void Close() override;

 private:
  base::File file_;
};

// Wraps a mojo::ScopedDataPipeProducerHandle target to write MHTML contents to.
// This implementation does not immediately finish and destroy itself due to
// the limited size of the data pipe buffer. We must ensure all data is
// written to the handle before finishing the write operation.
class MHTMLProducerHandleWriter : public MHTMLHandleWriter {
 public:
  MHTMLProducerHandleWriter(
      scoped_refptr<base::TaskRunner> main_thread_task_runner,
      MHTMLWriteCompleteCallback callback,
      mojo::ScopedDataPipeProducerHandle producer);

  MHTMLProducerHandleWriter(const MHTMLProducerHandleWriter&) = delete;
  MHTMLProducerHandleWriter& operator=(const MHTMLProducerHandleWriter&) =
      delete;

  ~MHTMLProducerHandleWriter() override;

 protected:
  // Creates a new SequencedTaskRunner to dispatch |watcher_| invocations on.
  void WriteContentsImpl(
      std::unique_ptr<MHTMLHandleWriter> self,
      std::vector<blink::WebThreadSafeData> mhtml_contents) override;

  void Close() override;

 private:
  void BeginWriting(std::unique_ptr<MHTMLHandleWriter> self,
                    std::vector<blink::WebThreadSafeData> mhtml_contents);
  void OnWriteComplete(std::unique_ptr<MHTMLHandleWriter> self,
                       MojoResult result);

  std::unique_ptr<mojo::DataPipeProducer> producer_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MHTML_HANDLE_WRITER_H_
