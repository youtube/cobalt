// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares HttpCache::Transaction, a private class of HttpCache so
// it should only be included by http_cache.cc

#ifndef NET_HTTP_HTTP_CACHE_TRANSACTION_H_
#define NET_HTTP_HTTP_CACHE_TRANSACTION_H_

#include "net/http/http_cache.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"

namespace net {

class HttpResponseHeaders;
class PartialData;

// This is the transaction that is returned by the HttpCache transaction
// factory.
class HttpCache::Transaction : public HttpTransaction {
 public:
  Transaction(HttpCache* cache, bool enable_range_support);
  virtual ~Transaction();

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo*, CompletionCallback*, LoadLog*);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int RestartWithCertificate(X509Certificate* client_cert,
                                     CompletionCallback* callback);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual bool IsReadyToRestartForAuth();
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress(void) const;

  // The transaction has the following modes, which apply to how it may access
  // its cache entry.
  //
  //  o If the mode of the transaction is NONE, then it is in "pass through"
  //    mode and all methods just forward to the inner network transaction.
  //
  //  o If the mode of the transaction is only READ, then it may only read from
  //    the cache entry.
  //
  //  o If the mode of the transaction is only WRITE, then it may only write to
  //    the cache entry.
  //
  //  o If the mode of the transaction is READ_WRITE, then the transaction may
  //    optionally modify the cache entry (e.g., possibly corresponding to
  //    cache validation).
  //
  //  o If the mode of the transaction is UPDATE, then the transaction may
  //    update existing cache entries, but will never create a new entry or
  //    respond using the entry read from the cache.
  enum Mode {
    NONE            = 0,
    READ_META       = 1 << 0,
    READ_DATA       = 1 << 1,
    READ            = READ_META | READ_DATA,
    WRITE           = 1 << 2,
    READ_WRITE      = READ | WRITE,
    UPDATE          = READ_META | WRITE,  // READ_WRITE & ~READ_DATA
  };

  Mode mode() const { return mode_; }

  const std::string& key() const { return cache_key_; }

  // Associates this transaction with a cache entry.
  int AddToEntry();

  // Called by the HttpCache when the given disk cache entry becomes accessible
  // to the transaction.  Returns network error code.
  int EntryAvailable(ActiveEntry* entry);

  // This transaction is being deleted and we are not done writing to the cache.
  // We need to indicate that the response data was truncated.  Returns true on
  // success.
  bool AddTruncatedFlag();

 private:
  static const size_t kNumValidationHeaders = 2;
  // Helper struct to pair a header name with its value, for
  // headers used to validate cache entries.
  struct ValidationHeaders {
    ValidationHeaders() : initialized(false) {}

    std::string values[kNumValidationHeaders];
    bool initialized;
  };

  // This is a helper function used to trigger a completion callback.  It may
  // only be called if callback_ is non-null.
  void DoCallback(int rv);

  // This will trigger the completion callback if appropriate.
  int HandleResult(int rv);

  // Sets request_ and fields derived from it.
  void SetRequest(LoadLog* load_log, const HttpRequestInfo* request);

  // Returns true if the request should be handled exclusively by the network
  // layer (skipping the cache entirely).
  bool ShouldPassThrough();

  // Called to begin reading from the cache.  Returns network error code.
  int BeginCacheRead();

  // Called to begin validating the cache entry.  Returns network error code.
  int BeginCacheValidation();

  // Called to begin validating an entry that stores partial content.  Returns
  // a network error code.
  int BeginPartialCacheValidation();

  // Validates the entry headers against the requested range and continues with
  // the validation of the rest of the entry.  Returns a network error code.
  int ValidateEntryHeadersAndContinue(bool byte_range_requested);

  // Performs the cache validation for the next chunk of data stored by the
  // cache.  If this chunk is not currently stored, starts the network request
  // to fetch it.  Returns a network error code.
  int ContinuePartialCacheValidation();

  // Called to start requests which were given an "if-modified-since" or
  // "if-none-match" validation header by the caller (NOT when the request was
  // conditionalized internally in response to LOAD_VALIDATE_CACHE).
  // Returns a network error code.
  int BeginExternallyConditionalizedRequest();

  // Called to begin a network transaction.  Returns network error code.
  int BeginNetworkRequest();

  // Called to restart a network transaction after an error.  Returns network
  // error code.
  int RestartNetworkRequest();

  // Called to restart a network transaction with a client certificate.
  // Returns network error code.
  int RestartNetworkRequestWithCertificate(X509Certificate* client_cert);

  // Called to restart a network transaction with authentication credentials.
  // Returns network error code.
  int RestartNetworkRequestWithAuth(const std::wstring& username,
                                    const std::wstring& password);

  // Called to determine if we need to validate the cache entry before using it.
  bool RequiresValidation();

  // Called to make the request conditional (to ask the server if the cached
  // copy is valid).  Returns true if able to make the request conditional.
  bool ConditionalizeRequest();

  // Makes sure that a 206 response is expected.  Returns true on success.
  // On success, |partial_content| will be set to true if we are processing a
  // partial entry.
  bool ValidatePartialResponse(const HttpResponseHeaders* headers,
                               bool* partial_content);

  // Handles a response validation error by bypassing the cache.
  void IgnoreRangeRequest();

  // Reads data from the network.
  int ReadFromNetwork(IOBuffer* data, int data_len);

  // Reads data from the cache entry.
  int ReadFromEntry(IOBuffer* data, int data_len);

  // Called to populate response_ from the cache entry.
  int ReadResponseInfoFromEntry();

  // Called to write data to the cache entry.  If the write fails, then the
  // cache entry is destroyed.  Future calls to this function will just do
  // nothing without side-effect.  Returns a network error code.
  int WriteToEntry(int index, int offset, IOBuffer* data, int data_len,
                   CompletionCallback* callback);

  // Called to write response_ to the cache entry. |truncated| indicates if the
  // entry should be marked as incomplete.
  void WriteResponseInfoToEntry(bool truncated);

  // Called to append response data to the cache entry.  Returns a network error
  // code.
  int AppendResponseDataToEntry(IOBuffer* data, int data_len,
                                CompletionCallback* callback);

  // Called to truncate response content in the entry.
  void TruncateResponseData();

  // Called when we are done writing to the cache entry.
  void DoneWritingToEntry(bool success);

  // Deletes the current partial cache entry (sparse), and optionally removes
  // the control object (partial_).
  void DoomPartialEntry(bool delete_object);

  // Performs the needed work after receiving data from the network.
  int DoNetworkReadCompleted(int result);

  // Performs the needed work after receiving data from the network, when
  // working with range requests.
  int DoPartialNetworkReadCompleted(int result);

  // Performs the needed work after receiving data from the cache.
  int DoCacheReadCompleted(int result);

  // Performs the needed work after receiving data from the cache, when
  // working with range requests.
  int DoPartialCacheReadCompleted(int result);

  // Performs the needed work after writing data to the cache.
  int DoCacheWriteCompleted(int result);

  // Called to signal completion of the network transaction's Start method:
  void OnNetworkInfoAvailable(int result);

  // Called to signal completion of the network transaction's Read method:
  void OnNetworkReadCompleted(int result);

  // Called to signal completion of the cache's ReadData method:
  void OnCacheReadCompleted(int result);

  // Called to signal completion of the cache's WriteData method:
  void OnCacheWriteCompleted(int result);

  // Called to signal completion of the cache entry's ReadyForSparseIO method:
  void OnCacheEntryReady(int result);

  scoped_refptr<LoadLog> load_log_;
  const HttpRequestInfo* request_;
  scoped_ptr<HttpRequestInfo> custom_request_;
  // If extra_headers specified a "if-modified-since" or "if-none-match",
  // |external_validation_| contains the value of those headers.
  ValidationHeaders external_validation_;
  base::WeakPtr<HttpCache> cache_;
  HttpCache::ActiveEntry* entry_;
  scoped_ptr<HttpTransaction> network_trans_;
  CompletionCallback* callback_;  // Consumer's callback.
  HttpResponseInfo response_;
  HttpResponseInfo auth_response_;
  std::string cache_key_;
  Mode mode_;
  bool reading_;  // We are already reading.
  bool invalid_range_;  // We may bypass the cache for this request.
  bool enable_range_support_;
  bool truncated_;  // We don't have all the response data.
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  int read_offset_;
  int effective_load_flags_;
  scoped_ptr<PartialData> partial_;  // We are dealing with range requests.
  uint64 final_upload_progress_;
  CompletionCallbackImpl<Transaction> network_info_callback_;
  CompletionCallbackImpl<Transaction> network_read_callback_;
  scoped_refptr<CancelableCompletionCallback<Transaction> >
      cache_read_callback_;
  scoped_refptr<CancelableCompletionCallback<Transaction> >
      cache_write_callback_;
  scoped_refptr<CancelableCompletionCallback<Transaction> >
      entry_ready_callback_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_TRANSACTION_H_
