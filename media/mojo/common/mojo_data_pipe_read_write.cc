// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_data_pipe_read_write.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "media/mojo/common/mojo_pipe_read_write_util.h"

using media::mojo_pipe_read_write_util::IsPipeReadWriteError;

namespace media {

// MojoDataPipeReader

MojoDataPipeReader::MojoDataPipeReader(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : consumer_handle_(std::move(consumer_handle)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()) {
  DVLOG(1) << __func__;

  MojoResult result = pipe_watcher_.Watch(
      consumer_handle_.get(), MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
      base::BindRepeating(&MojoDataPipeReader::TryReadData,
                          base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __func__
             << ": Failed to start watching the pipe. result=" << result;
    consumer_handle_.reset();
  }
}

MojoDataPipeReader::~MojoDataPipeReader() {
  DVLOG(1) << __func__;
}

void MojoDataPipeReader::CompleteCurrentRead() {
  DVLOG(4) << __func__;
  DCHECK(done_cb_);
  current_buffer_ = nullptr;
  current_buffer_size_ = 0;
  std::move(done_cb_).Run(true);
}

void MojoDataPipeReader::Read(uint8_t* buffer,
                              uint32_t num_bytes,
                              DoneCB done_cb) {
  DVLOG(3) << __func__;
  // Read() can not be called when there is another reading request in process.
  DCHECK(!current_buffer_size_);
  DCHECK(done_cb);
  if (!num_bytes) {
    std::move(done_cb).Run(true);
    return;
  }

  if (!consumer_handle_.is_valid()) {
    VLOG(1) << __func__ << ": Data pipe was closed.";
    std::move(done_cb).Run(false);
    return;
  }

  current_buffer_size_ = num_bytes;
  current_buffer_ = buffer;
  bytes_read_ = 0;
  done_cb_ = std::move(done_cb);
  // Try reading data immediately to reduce latency.
  TryReadData(MOJO_RESULT_OK);
}

void MojoDataPipeReader::TryReadData(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    OnPipeError(result);
    return;
  }

  DCHECK_GT(current_buffer_size_, bytes_read_);
  uint32_t num_bytes = current_buffer_size_ - bytes_read_;
  if (current_buffer_) {
    result = consumer_handle_->ReadData(current_buffer_ + bytes_read_,
                                        &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  } else {
    result = consumer_handle_->ReadData(nullptr, &num_bytes,
                                        MOJO_READ_DATA_FLAG_DISCARD);
  }

  if (IsPipeReadWriteError(result)) {
    OnPipeError(result);
  } else {
    if (result == MOJO_RESULT_OK) {
      DCHECK_GT(num_bytes, 0u);
      bytes_read_ += num_bytes;
      if (bytes_read_ == current_buffer_size_) {
        CompleteCurrentRead();
        return;
      }
    }
    pipe_watcher_.ArmOrNotify();
  }
}

void MojoDataPipeReader::OnPipeError(MojoResult result) {
  DVLOG(1) << __func__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  consumer_handle_.reset();

  if (current_buffer_) {
    DVLOG(1) << __func__ << ": reading from data pipe failed. result=" << result
             << ", buffer size=" << current_buffer_size_
             << ", num_bytes(read)=" << bytes_read_;
    bytes_read_ = 0;
    current_buffer_ = nullptr;
    current_buffer_size_ = 0;
    DCHECK(done_cb_);
    std::move(done_cb_).Run(false);
  }
}

bool MojoDataPipeReader::IsPipeValid() const {
  return consumer_handle_.is_valid();
}

void MojoDataPipeReader::Close() {
  consumer_handle_.reset();
}

// MojoDataPipeWriter

MojoDataPipeWriter::MojoDataPipeWriter(
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : producer_handle_(std::move(producer_handle)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()) {
  DVLOG(1) << __func__;

  MojoResult result =
      pipe_watcher_.Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                          base::BindRepeating(&MojoDataPipeWriter::TryWriteData,
                                              base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __func__
             << ": Failed to start watching the pipe. result=" << result;
    producer_handle_.reset();
  }
}

MojoDataPipeWriter::~MojoDataPipeWriter() {
  DVLOG(1) << __func__;
}

void MojoDataPipeWriter::Write(const uint8_t* buffer,
                               uint32_t buffer_size,
                               DoneCB done_cb) {
  DVLOG(3) << __func__;
  // Write() can not be called when another writing request is in process.
  DCHECK(!current_buffer_);
  DCHECK(done_cb);
  if (!buffer_size) {
    std::move(done_cb).Run(true);
    return;
  }
  DCHECK(buffer);

  // Cannot write if the pipe is already closed.
  if (!producer_handle_.is_valid()) {
    DVLOG(1) << __func__
             << ": Failed to write buffer because the pipe is already closed";
    std::move(done_cb).Run(false);
    return;
  }

  current_buffer_ = buffer;
  current_buffer_size_ = buffer_size;
  bytes_written_ = 0;
  done_cb_ = std::move(done_cb);
  // Try writing data immediately to reduce latency.
  TryWriteData(MOJO_RESULT_OK);
}

void MojoDataPipeWriter::TryWriteData(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    OnPipeError(result);
    return;
  }

  DCHECK(current_buffer_);
  DCHECK_GT(current_buffer_size_, bytes_written_);
  uint32_t num_bytes = current_buffer_size_ - bytes_written_;

  result = producer_handle_->WriteData(current_buffer_ + bytes_written_,
                                       &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  if (IsPipeReadWriteError(result)) {
    OnPipeError(result);
  } else {
    if (result == MOJO_RESULT_OK) {
      DCHECK_GT(num_bytes, 0u);
      bytes_written_ += num_bytes;
      if (bytes_written_ == current_buffer_size_) {
        CompleteCurrentWrite();
        return;
      }
    }
    pipe_watcher_.ArmOrNotify();
  }
}

void MojoDataPipeWriter::CompleteCurrentWrite() {
  DVLOG(4) << __func__;
  DCHECK(done_cb_);
  current_buffer_ = nullptr;
  std::move(done_cb_).Run(true);
}

void MojoDataPipeWriter::OnPipeError(MojoResult result) {
  DVLOG(1) << __func__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  producer_handle_.reset();

  if (current_buffer_) {
    DVLOG(1) << __func__ << ": writing to data pipe failed. result=" << result
             << ", buffer size=" << current_buffer_size_
             << ", num_bytes(written)=" << bytes_written_;
    current_buffer_ = nullptr;
    current_buffer_size_ = 0;
    bytes_written_ = 0;
    DCHECK(done_cb_);
    std::move(done_cb_).Run(false);
  }
}

bool MojoDataPipeWriter::IsPipeValid() const {
  return producer_handle_.is_valid();
}

void MojoDataPipeWriter::Close() {
  producer_handle_.reset();
}

}  // namespace media
