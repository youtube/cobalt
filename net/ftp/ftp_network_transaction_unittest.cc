// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_request_info.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Size we use for IOBuffers used to receive data from the test data socket.
const int kBufferSize = 128;

}  // namespace

namespace net {

class FtpSocketDataProvider : public DynamicSocketDataProvider {
 public:
  enum State {
    NONE,
    PRE_USER,
    PRE_PASSWD,
    PRE_SYST,
    PRE_PWD,
    PRE_TYPE,
    PRE_PASV,
    PRE_SIZE,
    PRE_MDTM,
    PRE_LIST,
    PRE_RETR,
    PRE_PASV2,
    PRE_CWD,
    PRE_QUIT,
    QUIT
  };

  FtpSocketDataProvider()
      : failure_injection_state_(NONE),
        multiline_welcome_(false) {
    Init();
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify("USER anonymous\r\n", data, PRE_PASSWD,
                      "331 Password needed\r\n");
      case PRE_PASSWD:
        {
          const char* response_one = "230 Welcome\r\n";
          const char* response_multi = "230- One\r\n230- Two\r\n230 Three\r\n";
          return Verify("PASS chrome@example.com\r\n", data, PRE_SYST,
                        multiline_welcome_ ? response_multi : response_one);
        }
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 UNIX\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"/\" is your current location\r\n");
      case PRE_TYPE:
        return Verify("TYPE I\r\n", data, PRE_PASV,
                      "200 TYPE is now 8-bit binary\r\n");
      case PRE_PASV:
        return Verify("PASV\r\n", data, PRE_SIZE,
                      "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
      case PRE_PASV2:
        // Parser should also accept format without parentheses.
        return Verify("PASV\r\n", data, PRE_CWD,
                      "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
      case PRE_QUIT:
        return Verify("QUIT\r\n", data, QUIT, "221 Goodbye.\r\n");
      default:
        NOTREACHED() << "State not handled " << state();
        return MockWriteResult(true, ERR_UNEXPECTED);
    }
  }

  void InjectFailure(State state, State next_state, const char* response) {
    DCHECK_EQ(NONE, failure_injection_state_);
    DCHECK_NE(NONE, state);
    DCHECK_NE(NONE, next_state);
    DCHECK_NE(state, next_state);
    failure_injection_state_ = state;
    failure_injection_next_state_ = next_state;
    fault_response_ = response;
  }

  State state() const {
    return state_;
  }

  virtual void Reset() {
    DynamicSocketDataProvider::Reset();
    Init();
  }

  void set_multiline_welcome(bool multiline) {
    multiline_welcome_ = multiline;
  }

 protected:
  void Init() {
    state_ = PRE_USER;
    SimulateRead("220 host TestFTPd\r\n");
  }

  // If protocol fault injection has been requested, adjusts state and mocked
  // read and returns true.
  bool InjectFault() {
    if (state_ != failure_injection_state_)
      return false;
    SimulateRead(fault_response_);
    state_ = failure_injection_next_state_;
    return true;
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read) {
    EXPECT_EQ(expected, data);
    if (expected == data) {
      state_ = next_state;
      SimulateRead(next_read);
      return MockWriteResult(true, data.length());
    }
    return MockWriteResult(true, ERR_UNEXPECTED);
  }

 private:
  State state_;
  State failure_injection_state_;
  State failure_injection_next_state_;
  const char* fault_response_;

  // If true, we will send multiple 230 lines as response after PASS.
  bool multiline_welcome_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProvider);
};

class FtpSocketDataProviderDirectoryListing : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderDirectoryListing() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data, PRE_MDTM,
                      "550 I can only retrieve regular files\r\n");
      case PRE_MDTM:
        return Verify("MDTM /\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR /\r\n", data, PRE_PASV2,
                      "550 Can't download directory\r\n");

      case PRE_CWD:
        return Verify("CWD /\r\n", data, PRE_LIST, "200 OK\r\n");
      case PRE_LIST:
        // TODO(phajdan.jr): Also test with "150 Accepted Data Connection".
        return Verify("LIST\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderDirectoryListing);
};

class FtpSocketDataProviderVMSDirectoryListing : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSDirectoryListing() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT:[000000]dir\r\n", data, PRE_MDTM,
                      "550 I can only retrieve regular files\r\n");
      case PRE_MDTM:
        return Verify("MDTM ANONYMOUS_ROOT:[000000]dir\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR ANONYMOUS_ROOT:[000000]dir\r\n", data, PRE_PASV2,
                      "550 Can't download directory\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[dir]\r\n", data, PRE_LIST,
                      "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST *.*;0\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderVMSDirectoryListing);
};

class FtpSocketDataProviderVMSDirectoryListingRootDirectory
    : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSDirectoryListingRootDirectory() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT\r\n", data, PRE_MDTM,
                      "550 I can only retrieve regular files\r\n");
      case PRE_MDTM:
        return Verify("MDTM ANONYMOUS_ROOT\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR ANONYMOUS_ROOT\r\n", data, PRE_PASV2,
                      "550 Can't download directory\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[000000]\r\n", data, PRE_LIST,
                      "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST *.*;0\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderVMSDirectoryListingRootDirectory);
};

class FtpSocketDataProviderFileDownload : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileDownload() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_MDTM,
                      "213 18\r\n");
      case PRE_MDTM:
        return Verify("MDTM /file\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownload);
};

class FtpSocketDataProviderVMSFileDownload : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSFileDownload() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_MDTM,
                      "213 18\r\n");
      case PRE_MDTM:
        return Verify("MDTM ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_QUIT,
                      "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderVMSFileDownload);
};

class FtpSocketDataProviderEscaping : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderEscaping() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE / !\"#$%y\200\201\r\n", data, PRE_MDTM,
                      "213 18\r\n");
      case PRE_MDTM:
        return Verify("MDTM / !\"#$%y\200\201\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR / !\"#$%y\200\201\r\n", data, PRE_QUIT,
                      "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEscaping);
};

class FtpSocketDataProviderFileDownloadAcceptedDataConnection
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadAcceptedDataConnection() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT,
                      "150 Accepted Data Connection\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderFileDownloadAcceptedDataConnection);
};

class FtpSocketDataProviderFileDownloadTransferStarting
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadTransferStarting() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT,
                      "125-Data connection already open.\r\n"
                      "125  Transfer starting.\r\n"
                      "226 Transfer complete.\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadTransferStarting);
};

class FtpSocketDataProviderDirectoryListingTransferStarting
    : public FtpSocketDataProviderDirectoryListing {
 public:
  FtpSocketDataProviderDirectoryListingTransferStarting() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_LIST:
        return Verify("LIST\r\n", data, PRE_QUIT,
                      "125-Data connection already open.\r\n"
                      "125  Transfer starting.\r\n"
                      "226 Transfer complete.\r\n");
      default:
        return FtpSocketDataProviderDirectoryListing::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderDirectoryListingTransferStarting);
};

class FtpSocketDataProviderFileDownloadInvalidResponse
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadInvalidResponse() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_QUIT,
                      "500 Evil Response\r\n"
                      "500 More Evil\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadInvalidResponse);
};

class FtpSocketDataProviderFileDownloadRetrFail
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadRetrFail() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_CWD:
        return Verify("CWD /file\r\n", data, PRE_QUIT,
                      "550 file is a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadRetrFail);
};

class FtpSocketDataProviderEvilPasv : public FtpSocketDataProviderFileDownload {
 public:
  explicit FtpSocketDataProviderEvilPasv(const char* pasv_response,
                                        State expected_state)
      : pasv_response_(pasv_response),
        expected_state_(expected_state) {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_PASV:
        return Verify("PASV\r\n", data, expected_state_, pasv_response_);
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* pasv_response_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilPasv);
};

class FtpSocketDataProviderEvilLogin : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilLogin(const char* expected_user,
                                const char* expected_password)
      : expected_user_(expected_user),
        expected_password_(expected_password) {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify(std::string("USER ") + expected_user_ + "\r\n", data,
                      PRE_PASSWD, "331 Password needed\r\n");
      case PRE_PASSWD:
        return Verify(std::string("PASS ") + expected_password_ + "\r\n", data,
                      PRE_SYST, "230 Welcome\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* expected_user_;
  const char* expected_password_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilLogin);
};

class FtpSocketDataProviderCloseConnection : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderCloseConnection() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify("USER anonymous\r\n", data,
                      PRE_QUIT, "");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderCloseConnection);
};

class FtpNetworkTransactionTest : public PlatformTest {
 public:
  FtpNetworkTransactionTest()
      : host_resolver_(new MockHostResolver),
        session_(new FtpNetworkSession(host_resolver_)),
        transaction_(session_.get(), &mock_socket_factory_) {
  }

 protected:
  FtpRequestInfo GetRequestInfo(const std::string& url) {
    FtpRequestInfo info;
    info.url = GURL(url);
    return info;
  }

  void ExecuteTransaction(FtpSocketDataProvider* ctrl_socket,
                          const char* request,
                          int expected_result) {
    std::string mock_data("mock-data");
    MockRead data_reads[] = {
      MockRead(mock_data.c_str()),
    };
    // For compatibility with FileZilla, the transaction code will use two data
    // sockets for directory requests. For more info see http://crbug.com/25316.
    StaticSocketDataProvider data1(data_reads, NULL);
    StaticSocketDataProvider data2(data_reads, NULL);
    mock_socket_factory_.AddSocketDataProvider(ctrl_socket);
    mock_socket_factory_.AddSocketDataProvider(&data1);
    mock_socket_factory_.AddSocketDataProvider(&data2);
    FtpRequestInfo request_info = GetRequestInfo(request);
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
    ASSERT_EQ(ERR_IO_PENDING,
              transaction_.Start(&request_info, &callback_, NULL));
    EXPECT_NE(LOAD_STATE_IDLE, transaction_.GetLoadState());
    EXPECT_EQ(expected_result, callback_.WaitForResult());
    EXPECT_EQ(FtpSocketDataProvider::QUIT, ctrl_socket->state());
    if (expected_result == OK) {
      scoped_refptr<IOBuffer> io_buffer(new IOBuffer(kBufferSize));
      memset(io_buffer->data(), 0, kBufferSize);
      ASSERT_EQ(ERR_IO_PENDING,
                transaction_.Read(io_buffer.get(), kBufferSize, &callback_));
      EXPECT_EQ(static_cast<int>(mock_data.length()),
                callback_.WaitForResult());
      EXPECT_EQ(mock_data, std::string(io_buffer->data(), mock_data.length()));
    }
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
  }

  void TransactionFailHelper(FtpSocketDataProvider* ctrl_socket,
                             const char* request,
                             FtpSocketDataProvider::State state,
                             FtpSocketDataProvider::State next_state,
                             const char* response,
                             int expected_result) {
    ctrl_socket->InjectFailure(state, next_state, response);
    ExecuteTransaction(ctrl_socket, request, expected_result);
  }

  scoped_refptr<MockHostResolver> host_resolver_;
  scoped_refptr<FtpNetworkSession> session_;
  MockClientSocketFactory mock_socket_factory_;
  FtpNetworkTransaction transaction_;
  TestCompletionCallback callback_;
};

TEST_F(FtpNetworkTransactionTest, FailedLookup) {
  FtpRequestInfo request_info = GetRequestInfo("ftp://badhost");
  host_resolver_->rules()->AddSimulatedFailure("badhost");
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, NULL));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransaction) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcome) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionShortReads2) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionShortReads5) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcomeShort) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // The client will not consume all three 230 lines. That's good, we want to
  // test that scenario.
  ctrl_socket.allow_unconsumed_reads(true);
  ctrl_socket.set_multiline_welcome(true);
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionVMS) {
  FtpSocketDataProviderVMSDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/dir", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionVMSRootDirectory) {
  FtpSocketDataProviderVMSDirectoryListingRootDirectory ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionTransferStarting) {
  FtpSocketDataProviderDirectoryListingTransferStarting ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransaction) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionMultilineWelcome) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionShortReads2) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionShortReads5) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionVMS) {
  FtpSocketDataProviderVMSFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionAcceptedDataConnection) {
  FtpSocketDataProviderFileDownloadAcceptedDataConnection ctrl_socket;
  std::string mock_data("mock-data");
  MockRead data_reads[] = {
    MockRead(mock_data.c_str()),
  };
  StaticSocketDataProvider data_socket1(data_reads, NULL);
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket);
  mock_socket_factory_.AddSocketDataProvider(&data_socket1);
  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  // Start the transaction.
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, NULL));
  EXPECT_EQ(OK, callback_.WaitForResult());

  // The transaction fires the callback when we can start reading data.
  EXPECT_EQ(FtpSocketDataProvider::PRE_QUIT, ctrl_socket.state());
  EXPECT_EQ(LOAD_STATE_SENDING_REQUEST, transaction_.GetLoadState());
  scoped_refptr<IOBuffer> io_buffer(new IOBuffer(kBufferSize));
  memset(io_buffer->data(), 0, kBufferSize);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Read(io_buffer.get(), kBufferSize, &callback_));
  EXPECT_EQ(LOAD_STATE_READING_RESPONSE, transaction_.GetLoadState());
  EXPECT_EQ(static_cast<int>(mock_data.length()),
            callback_.WaitForResult());
  EXPECT_EQ(LOAD_STATE_READING_RESPONSE, transaction_.GetLoadState());
  EXPECT_EQ(mock_data, std::string(io_buffer->data(), mock_data.length()));

  // FTP server should disconnect the data socket. It is also a signal for the
  // FtpNetworkTransaction that the data transfer is finished.
  ClientSocket* data_socket = mock_socket_factory_.GetMockTCPClientSocket(1);
  ASSERT_TRUE(data_socket);
  data_socket->Disconnect();

  // We should issue Reads until one returns EOF...
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Read(io_buffer.get(), kBufferSize, &callback_));

  // Make sure the transaction finishes cleanly.
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_EQ(FtpSocketDataProvider::QUIT, ctrl_socket.state());
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionTransferStarting) {
  FtpSocketDataProviderFileDownloadTransferStarting ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionInvalidResponse) {
  FtpSocketDataProviderFileDownloadInvalidResponse ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort1) {
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,0,22)\r\n",
                                           FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort2) {
  // Still unsafe. 1 * 256 + 2 = 258, which is < 1024.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,1,2)\r\n",
                                           FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort3) {
  // Still unsafe. 3 * 256 + 4 = 772, which is < 1024.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,3,4)\r\n",
                                           FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort4) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,8,1)\r\n",
                                           FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafeHost) {
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (10,1,2,3,4,123,456)\r\n", FtpSocketDataProvider::PRE_SIZE);
  std::string mock_data("mock-data");
  MockRead data_reads[] = {
    MockRead(mock_data.c_str()),
  };
  StaticSocketDataProvider data_socket1(data_reads, NULL);
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket);
  mock_socket_factory_.AddSocketDataProvider(&data_socket1);
  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  // Start the transaction.
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, NULL));
  EXPECT_EQ(OK, callback_.WaitForResult());

  // The transaction fires the callback when we can start reading data. That
  // means that the data socket should be open.
  MockTCPClientSocket* data_socket =
      mock_socket_factory_.GetMockTCPClientSocket(1);
  ASSERT_TRUE(data_socket);
  ASSERT_TRUE(data_socket->IsConnected());

  // Even if the PASV response specified some other address, we connect
  // to the address we used for control connection.
  EXPECT_EQ("127.0.0.1", NetAddressToString(data_socket->addresses().head()));

  // Make sure we have only one host entry in the AddressList.
  EXPECT_FALSE(data_socket->addresses().head()->ai_next);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadUsername) {
  FtpSocketDataProviderEvilLogin ctrl_socket("hello%0Aworld", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%0Aworld:test@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadPassword) {
  FtpSocketDataProviderEvilLogin ctrl_socket("test", "hello%0Dworld");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%0Dworld@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionSpaceInLogin) {
  FtpSocketDataProviderEvilLogin ctrl_socket("hello world", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%20world:test@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionSpaceInPassword) {
  FtpSocketDataProviderEvilLogin ctrl_socket("test", "hello world");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%20world@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, EvilRestartUser) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.InjectFailure(FtpSocketDataProvider::PRE_PASSWD,
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, NULL));
  EXPECT_EQ(ERR_FAILED, callback_.WaitForResult());

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(false, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, ctrl_writes);
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING, transaction_.RestartWithAuth(L"foo\nownz0red",
                                                         L"innocent",
                                                         &callback_));
  EXPECT_EQ(ERR_MALFORMED_IDENTITY, callback_.WaitForResult());
}

TEST_F(FtpNetworkTransactionTest, EvilRestartPassword) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.InjectFailure(FtpSocketDataProvider::PRE_PASSWD,
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, NULL));
  EXPECT_EQ(ERR_FAILED, callback_.WaitForResult());

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("331 User okay, send password\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(false, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("USER innocent\r\n"),
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, ctrl_writes);
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING, transaction_.RestartWithAuth(L"innocent",
                                                         L"foo\nownz0red",
                                                         &callback_));
  EXPECT_EQ(ERR_MALFORMED_IDENTITY, callback_.WaitForResult());
}

TEST_F(FtpNetworkTransactionTest, Escaping) {
  FtpSocketDataProviderEscaping ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/%20%21%22%23%24%25%79%80%81",
                     OK);
}

// Regression test for http://crbug.com/25023.
TEST_F(FtpNetworkTransactionTest, CloseConnection) {
  FtpSocketDataProviderCloseConnection ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_EMPTY_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailUser) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_USER,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 no such user\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPass) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailSyst) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_SYST,
                        FtpSocketDataProvider::PRE_PWD,
                        "500 failed syst\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPwd) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed pwd\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailType) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_TYPE,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed type\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPasv) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASV,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed pasv\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMalformedMdtm) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_MDTM,
                        FtpSocketDataProvider::PRE_RETR,
                        "213 foobar\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailMdtm) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_MDTM,
                        FtpSocketDataProvider::PRE_RETR,
                        "500 failed mdtm\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPasv2) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASV2,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed pasv2\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailCwd) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_CWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed cwd\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFileNotFound) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_CWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "550 cannot open file\r\n",
                        ERR_FILE_NOT_FOUND);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailList) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_LIST,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed list\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailUser) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_USER,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 no such user\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPass) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailSyst) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_SYST,
                        FtpSocketDataProvider::PRE_PWD,
                        "500 failed syst\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPwd) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed pwd\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailType) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_TYPE,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed type\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPasv) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PASV,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed pasv\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailMdtm) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_MDTM,
                        FtpSocketDataProvider::PRE_RETR,
                        "500 failed mdtm\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailRetr) {
  FtpSocketDataProviderFileDownloadRetrFail ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_RETR,
                        FtpSocketDataProvider::PRE_QUIT,
                        "500 failed retr\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFileNotFound) {
  FtpSocketDataProviderFileDownloadRetrFail ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_RETR,
                        FtpSocketDataProvider::PRE_PASV2,
                        "550 cannot open file\r\n",
                        ERR_FILE_NOT_FOUND);
}

}  // namespace net
