// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "build/build_config.h"

#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
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
    PRE_SIZE,
    PRE_EPSV,
    PRE_PASV,
    PRE_MLSD,
    PRE_LIST,
    PRE_RETR,
    PRE_CWD,
    PRE_QUIT,
    PRE_NOPASV,
    QUIT
  };

  FtpSocketDataProvider() {
    Init();
    AddDefaultTransitions();
  }

  void AddTransition(State from, const std::string& input,
                     State to, const std::string& response) {
    transitions_[from] = Transition(input, to, response);
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (!ContainsKey(transitions_, state())) {
      NOTREACHED() << "State not handled: " << state();
      return MockWriteResult(true, ERR_UNEXPECTED);
    }

    Transition t(transitions_[state()]);
    return Verify(t.input, data, t.state, t.response);
  }

  void PrepareMockDirectoryListing() {
    AddTransition(PRE_SIZE, "SIZE /\r\n",
                  PRE_CWD, "550 I can only retrieve regular files\r\n");
    AddTransition(PRE_CWD, "CWD /\r\n",
                  PRE_MLSD, "200 OK\r\n");
    AddTransition(PRE_MLSD, "MLSD\r\n",
                  PRE_QUIT, "150 Accepted data connection\r\n"
                            "226 MLSD complete\r\n");
    AddTransition(PRE_LIST, "LIST\r\n",
                  PRE_QUIT, "200 OK\r\n");
  }

  void PrepareMockDirectoryListingWithPasvFallback() {
    PrepareMockDirectoryListing();
    AddTransition(PRE_EPSV, "EPSV\r\n",
                  PRE_PASV, "500 Command not understood\r\n");
    AddTransition(PRE_PASV, "PASV\r\n",
                  PRE_SIZE, "227 Passive Mode 127,0,0,1,123,456\r\n");
  }

  void PrepareMockVMSDirectoryListing() {
    AddTransition(PRE_SYST, "SYST\r\n",
                  PRE_PWD, "215 VMS\r\n");
    AddTransition(PRE_PWD, "PWD\r\n",
                  PRE_TYPE, "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
    AddTransition(PRE_EPSV, "EPSV\r\n",
                  PRE_PASV, "500 Invalid command\r\n");
    AddTransition(PRE_PASV, "PASV\r\n",
                  PRE_SIZE, "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
    AddTransition(PRE_SIZE, "SIZE ANONYMOUS_ROOT:[000000]dir\r\n",
                  PRE_CWD, "550 I can only retrieve regular files\r\n");
    AddTransition(PRE_CWD, "CWD ANONYMOUS_ROOT:[dir]\r\n",
                  PRE_MLSD, "200 OK\r\n");
    AddTransition(PRE_MLSD, "MLSD\r\n",
                  PRE_LIST, "500 Invalid command\r\n");
    AddTransition(PRE_LIST, "LIST *.*;0\r\n",
                  PRE_QUIT, "200 OK\r\n");
  }

  void PrepareMockVMSDirectoryListingRootDirectory() {
    PrepareMockVMSDirectoryListing();
    AddTransition(PRE_SIZE, "SIZE ANONYMOUS_ROOT\r\n",
                  PRE_CWD, "550 I can only retrieve regular files\r\n");
    AddTransition(PRE_CWD, "CWD ANONYMOUS_ROOT:[000000]\r\n",
                  PRE_MLSD, "200 OK\r\n");
  }

  void PrepareMockFileDownload() {
    AddTransition(PRE_SIZE, "SIZE /file\r\n",
                  PRE_RETR, "213 18\r\n");
    AddTransition(PRE_RETR, "RETR /file\r\n",
                  PRE_QUIT, "200 OK\r\n");
  }

  void PrepareMockFileDownloadWithPasvFallback() {
    PrepareMockFileDownload();
    AddTransition(PRE_EPSV, "EPSV\r\n",
                  PRE_PASV, "500 Invalid command\r\n");
    AddTransition(PRE_PASV, "PASV\r\n",
                  PRE_SIZE, "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
  }

  void PrepareMockVMSFileDownload() {
    AddTransition(PRE_SYST, "SYST\r\n",
                  PRE_PWD, "215 VMS\r\n");
    AddTransition(PRE_PWD, "PWD\r\n",
                  PRE_TYPE, "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
    AddTransition(PRE_EPSV, "EPSV\r\n",
                  PRE_PASV, "500 Invalid command\r\n");
    AddTransition(PRE_PASV, "PASV\r\n",
                  PRE_SIZE, "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
    AddTransition(PRE_SIZE, "SIZE ANONYMOUS_ROOT:[000000]file\r\n",
                  PRE_RETR, "213 18\r\n");
    AddTransition(PRE_RETR, "RETR ANONYMOUS_ROOT:[000000]file\r\n",
                  PRE_QUIT, "200 OK\r\n");
  }

  void PrepareMockFileDownloadEscaping() {
    PrepareMockFileDownload();
    AddTransition(PRE_SIZE, "SIZE / !\"#$%y\200\201\r\n",
                  PRE_RETR, "213 18\r\n");
    AddTransition(PRE_RETR, "RETR / !\"#$%y\200\201\r\n",
                  PRE_QUIT, "200 OK\r\n");
  }

  void PrepareMockFileDownloadTransferStarting() {
    PrepareMockFileDownload();
    AddTransition(PRE_RETR, "RETR /file\r\n",
                  PRE_QUIT, "125-Data connection already open.\r\n"
                  "125  Transfer starting.\r\n"
                  "226 Transfer complete.\r\n");
  }

  void PrepareMockDirectoryListingTransferStarting() {
    PrepareMockDirectoryListing();
    AddTransition(PRE_LIST, "LIST\r\n",
                  PRE_QUIT, "125-Data connection already open.\r\n"
                  "125  Transfer starting.\r\n"
                  "226 Transfer complete.\r\n");
  }

  void PrepareMockFileDownloadEvilEpsv(const std::string& epsv_response,
                                       State next_state) {
    PrepareMockFileDownload();
    AddTransition(PRE_EPSV, "EPSV\r\n",
                  next_state, epsv_response);
  }

  void PrepareMockFileDownloadEvilPasv(const std::string& pasv_response,
                                       State next_state) {
    PrepareMockFileDownloadWithPasvFallback();
    AddTransition(PRE_PASV, "PASV\r\n",
                  next_state, pasv_response);
  }

  void PrepareMockFileDownloadEvilSize(const std::string& size_response,
                                       State next_state) {
    PrepareMockFileDownload();
    AddTransition(PRE_SIZE, "SIZE /file\r\n",
                  next_state, size_response);
  }

  void PrepareMockFileDownloadEvilLogin(const std::string& user,
                                        const std::string& password) {
    PrepareMockFileDownload();
    AddTransition(PRE_USER, std::string("USER ") + user + "\r\n",
                  PRE_PASSWD, "331 Password needed\r\n");
    AddTransition(PRE_PASSWD, std::string("PASS ") + password + "\r\n",
                  PRE_SYST, "230 Welcome\r\n");
  }

  State state() const {
    return state_;
  }

  virtual void Reset() {
    DynamicSocketDataProvider::Reset();
    Init();
  }

 protected:
  void Init() {
    state_ = PRE_USER;
    SimulateRead("220 host TestFTPd\r\n");
  }

  void AddDefaultTransitions() {
    AddTransition(PRE_USER, "USER anonymous\r\n",
                  PRE_PASSWD, "331 Password needed\r\n");
    AddTransition(PRE_PASSWD, "PASS chrome@example.com\r\n",
                  PRE_SYST, "230 Welcome!\r\n");
    AddTransition(PRE_SYST, "SYST\r\n",
                  PRE_PWD, "215 UNIX\r\n");
    AddTransition(PRE_PWD, "PWD\r\n",
                  PRE_TYPE, "257 \"/\" is your current location\r\n");
    AddTransition(PRE_TYPE, "TYPE I\r\n",
                  PRE_EPSV, "200 TYPE set successfully\r\n");
    AddTransition(PRE_EPSV, "EPSV\r\n",
                  PRE_SIZE, "227 Entering Passive Mode (|||31744|)\r\n");
    // Use unallocated 599 FTP error code to make sure it falls into the
    // generic ERR_FTP_FAILED bucket.
    AddTransition(PRE_NOPASV, "PASV\r\n",
                  PRE_QUIT, "599 fail\r\n");
    AddTransition(PRE_QUIT, "QUIT\r\n",
                  QUIT, "221 Goodbye.\r\n");
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read,
                         const size_t next_read_length) {
    EXPECT_EQ(expected, data);
    if (expected == data) {
      state_ = next_state;
      SimulateRead(next_read, next_read_length);
      return MockWriteResult(true, data.length());
    }
    return MockWriteResult(true, ERR_UNEXPECTED);
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const std::string& next_read) {
    return Verify(expected, data, next_state,
                  next_read.data(), next_read.size());
  }

 private:
  struct Transition {
    Transition() : state(NONE) {
    }

    Transition(const std::string& i, State s, const std::string& r)
        : input(i),
          state(s),
          response(r) {
    }

    std::string input;
    State state;
    std::string response;
  };

  State state_;

  std::map<State, Transition> transitions_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProvider);
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
      // Usually FTP servers close the data connection after the entire data has
      // been received.
      MockRead(false, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(mock_data.c_str()),
    };
    StaticSocketDataProvider data_socket(data_reads, arraysize(data_reads),
                                         NULL, 0);
    mock_socket_factory_.AddSocketDataProvider(ctrl_socket);
    mock_socket_factory_.AddSocketDataProvider(&data_socket);
    FtpRequestInfo request_info = GetRequestInfo(request);
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
    ASSERT_EQ(ERR_IO_PENDING,
              transaction_.Start(&request_info, &callback_, BoundNetLog()));
    EXPECT_NE(LOAD_STATE_IDLE, transaction_.GetLoadState());
    ASSERT_EQ(expected_result, callback_.WaitForResult());
    if (expected_result == OK) {
      scoped_refptr<IOBuffer> io_buffer(new IOBuffer(kBufferSize));
      memset(io_buffer->data(), 0, kBufferSize);
      ASSERT_EQ(ERR_IO_PENDING,
                transaction_.Read(io_buffer.get(), kBufferSize, &callback_));
      ASSERT_EQ(static_cast<int>(mock_data.length()),
                callback_.WaitForResult());
      EXPECT_EQ(mock_data, std::string(io_buffer->data(), mock_data.length()));

      // Do another Read to detect that the data socket is now closed.
      int rv = transaction_.Read(io_buffer.get(), kBufferSize, &callback_);
      if (rv == ERR_IO_PENDING) {
        EXPECT_EQ(0, callback_.WaitForResult());
      } else {
        EXPECT_EQ(0, rv);
      }
    }
    EXPECT_EQ(FtpSocketDataProvider::QUIT, ctrl_socket->state());
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
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
            transaction_.Start(&request_info, &callback_, BoundNetLog()));
  ASSERT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
}

// Check that when determining the host, the square brackets decorating IPv6
// literals in URLs are stripped.
TEST_F(FtpNetworkTransactionTest, StripBracketsFromIPv6Literals) {
  host_resolver_->rules()->AddSimulatedFailure("[::1]");

  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilSize(
      "213 99999999999999999999999999999999\r\n",
      FtpSocketDataProvider::PRE_QUIT);

  // We start a transaction that is expected to fail with ERR_INVALID_RESPONSE.
  // The important part of this test is to make sure that we don't fail with
  // ERR_NAME_NOT_RESOLVED, since that would mean the decorated hostname
  // was used.
  ExecuteTransaction(&ctrl_socket, "ftp://[::1]/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransaction) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);

  EXPECT_TRUE(transaction_.GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionWithPasvFallback) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListingWithPasvFallback();

  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);

  EXPECT_TRUE(transaction_.GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionWithTypecode) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ExecuteTransaction(&ctrl_socket, "ftp://host;type=d", OK);

  EXPECT_TRUE(transaction_.GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcome) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                            "PASS chrome@example.com\r\n",
                            FtpSocketDataProvider::PRE_SYST,
                            "230-Welcome!\r\n"
                            "230-Second line\r\n"
                            "230 Last line\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionShortReads2) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionShortReads5) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcomeShort) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                            "PASS chrome@example.com\r\n",
                            FtpSocketDataProvider::PRE_SYST,
                            "230-Welcome!\r\n"
                            "230-Second line\r\n"
                            "230 Last line\r\n");

  // The client will not consume all three 230 lines. That's good, we want to
  // test that scenario.
  ctrl_socket.allow_unconsumed_reads(true);
  ctrl_socket.set_short_read_limit(5);

  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionVMS) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockVMSDirectoryListing();
  ExecuteTransaction(&ctrl_socket, "ftp://host/dir", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionVMSRootDirectory) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockVMSDirectoryListingRootDirectory();
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionTransferStarting) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListingTransferStarting();
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransaction) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionWithPasvFallback) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadWithPasvFallback();
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionWithTypecodeA) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_TYPE,
                            "TYPE A\r\n",
                            FtpSocketDataProvider::PRE_EPSV,
                            "200 TYPE set successfully\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=a", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionWithTypecodeI) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=i", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionMultilineWelcome) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                            "PASS chrome@example.com\r\n",
                            FtpSocketDataProvider::PRE_SYST,
                            "230-Welcome!\r\n"
                            "230-Second line\r\n"
                            "230 Last line\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionShortReads2) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionShortReads5) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionVMS) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockVMSFileDownload();
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionTransferStarting) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadTransferStarting();
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionInvalidResponse) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();
  // Use unallocated 599 FTP error code to make sure it falls into the
  // generic ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_SIZE,
                            "SIZE /file\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 Evil Response\r\n"
                            "599 More Evil\r\n");
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvReallyBadFormat) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilPasv("227 Portscan (127,0,0,\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort1) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilPasv(
      "227 Portscan (127,0,0,1,0,22)\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort2) {
  FtpSocketDataProvider ctrl_socket;
  // Still unsafe. 1 * 256 + 2 = 258, which is < 1024.
  ctrl_socket.PrepareMockFileDownloadEvilPasv(
      "227 Portscan (127,0,0,1,1,2)\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort3) {
  FtpSocketDataProvider ctrl_socket;
  // Still unsafe. 3 * 256 + 4 = 772, which is < 1024.
  ctrl_socket.PrepareMockFileDownloadEvilPasv(
      "227 Portscan (127,0,0,1,3,4)\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort4) {
  FtpSocketDataProvider ctrl_socket;
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  ctrl_socket.PrepareMockFileDownloadEvilPasv(
      "227 Portscan (127,0,0,1,8,1)\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafeHost) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilPasv(
      "227 Portscan (10,1,2,3,4,123,456)\r\n",
      FtpSocketDataProvider::PRE_SIZE);
  std::string mock_data("mock-data");
  MockRead data_reads[] = {
    MockRead(mock_data.c_str()),
  };
  StaticSocketDataProvider data_socket1(data_reads, arraysize(data_reads),
                                        NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket);
  mock_socket_factory_.AddSocketDataProvider(&data_socket1);
  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  // Start the transaction.
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, BoundNetLog()));
  ASSERT_EQ(OK, callback_.WaitForResult());

  // The transaction fires the callback when we can start reading data. That
  // means that the data socket should be open.
  MockTCPClientSocket* data_socket =
      mock_socket_factory_.GetMockTCPClientSocket(1);
  ASSERT_TRUE(data_socket);
  ASSERT_TRUE(data_socket->IsConnected());

  // Even if the PASV response specified some other address, we connect
  // to the address we used for control connection (which could be 127.0.0.1
  // or ::1 depending on whether we use IPv6).
  const struct addrinfo* addrinfo = data_socket->addresses().head();
  while (addrinfo) {
    EXPECT_NE("1.2.3.4", NetAddressToString(addrinfo));
    addrinfo = addrinfo->ai_next;
  }
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat1) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (|||22)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat2) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (||\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat3) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat4) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (||||)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat5) {
  const char response[] = "227 Portscan (\0\0\031773\0)\r\n";
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv(
      std::string(response, sizeof(response) - 1),
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort1) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (|||22|)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort2) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (|||258|)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort3) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (|||772|)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort4) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan (|||2049|)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvWeirdSep) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan ($$$31744$)\r\n",
                                              FtpSocketDataProvider::PRE_SIZE);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest,
       DownloadTransactionEvilEpsvWeirdSepUnsafePort) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv("227 Portscan ($$$317$)\r\n",
                                              FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvIllegalHost) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilEpsv(
      "227 Portscan (|2|::1|31744|)\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadUsername) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilLogin("hello%0Aworld", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%0Aworld:test@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadPassword) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilLogin("test", "hello%0Dworld");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%0Dworld@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionSpaceInLogin) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilLogin("hello world", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%20world:test@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionSpaceInPassword) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilLogin("test", "hello world");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%20world@host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, EvilRestartUser) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                             "PASS chrome@example.com\r\n",
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, BoundNetLog()));
  ASSERT_EQ(ERR_FTP_FAILED, callback_.WaitForResult());

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(false, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, arraysize(ctrl_reads),
                                        ctrl_writes, arraysize(ctrl_writes));
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.RestartWithAuth(ASCIIToUTF16("foo\nownz0red"),
                                         ASCIIToUTF16("innocent"),
                                         &callback_));
  EXPECT_EQ(ERR_MALFORMED_IDENTITY, callback_.WaitForResult());
}

TEST_F(FtpNetworkTransactionTest, EvilRestartPassword) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                             "PASS chrome@example.com\r\n",
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, &callback_, BoundNetLog()));
  ASSERT_EQ(ERR_FTP_FAILED, callback_.WaitForResult());

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
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, arraysize(ctrl_reads),
                                        ctrl_writes, arraysize(ctrl_writes));
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.RestartWithAuth(ASCIIToUTF16("innocent"),
                                         ASCIIToUTF16("foo\nownz0red"),
                                         &callback_));
  EXPECT_EQ(ERR_MALFORMED_IDENTITY, callback_.WaitForResult());
}

TEST_F(FtpNetworkTransactionTest, Escaping) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEscaping();
  ExecuteTransaction(&ctrl_socket, "ftp://host/%20%21%22%23%24%25%79%80%81",
                     OK);
}

// Test for http://crbug.com/23794.
TEST_F(FtpNetworkTransactionTest, DownloadTransactionEvilSize) {
  FtpSocketDataProvider ctrl_socket;
  // Try to overflow int64 in the response.
  ctrl_socket.PrepareMockFileDownloadEvilSize(
      "213 99999999999999999999999999999999\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

// Test for http://crbug.com/36360.
TEST_F(FtpNetworkTransactionTest, DownloadTransactionBigSize) {
  // Pass a valid, but large file size. The transaction should not fail.
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownloadEvilSize(
      "213 3204427776\r\n",
      FtpSocketDataProvider::PRE_RETR);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
  EXPECT_EQ(3204427776LL,
            transaction_.GetResponseInfo()->expected_content_size);
}

// Regression test for http://crbug.com/25023.
TEST_F(FtpNetworkTransactionTest, CloseConnection) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_USER,
                            "USER anonymous\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "");
  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_EMPTY_RESPONSE);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailUser) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_USER,
                            "USER anonymous\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPass) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                            "PASS chrome@example.com\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "530 Login authentication failed\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

// Regression test for http://crbug.com/38707.
TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPass503) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                            "PASS chrome@example.com\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "503 Bad sequence of commands\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_BAD_COMMAND_SEQUENCE);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailSyst) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_SYST,
                            "SYST\r\n",
                            FtpSocketDataProvider::PRE_PWD,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPwd) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PWD,
                            "PWD\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailType) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_TYPE,
                            "TYPE I\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailEpsv) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_EPSV,
                            "EPSV\r\n",
                            FtpSocketDataProvider::PRE_NOPASV,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailCwd) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_CWD,
                            "CWD /\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFileNotFound) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_CWD,
                            "CWD /\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "550 Cannot open file\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FILE_NOT_FOUND);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailMlsd) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_MLSD,
                            "MLSD\r\n",
                            FtpSocketDataProvider::PRE_LIST,
                            "500 Unrecognized command\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailList) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockDirectoryListing();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_MLSD,
                            "MLSD\r\n",
                            FtpSocketDataProvider::PRE_LIST,
                            "500 Unrecognized command\r\n");

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_LIST,
                            "LIST\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailUser) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_USER,
                            "USER anonymous\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPass) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PASSWD,
                            "PASS chrome@example.com\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "530 Login authentication failed\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailSyst) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_SYST,
                            "SYST\r\n",
                            FtpSocketDataProvider::PRE_PWD,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPwd) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PWD,
                            "PWD\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailType) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_TYPE,
                            "TYPE I\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailEpsv) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_EPSV,
                            "EPSV\r\n",
                            FtpSocketDataProvider::PRE_NOPASV,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailRetr) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_RETR,
                            "RETR /file\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "599 fail\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFileNotFound) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_SIZE,
                            "SIZE /file\r\n",
                            FtpSocketDataProvider::PRE_QUIT,
                            "550 File Not Found\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=i", ERR_FTP_FAILED);
}

// Test for http://crbug.com/38845.
TEST_F(FtpNetworkTransactionTest, ZeroLengthDirInPWD) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.PrepareMockFileDownload();

  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  ctrl_socket.AddTransition(FtpSocketDataProvider::PRE_PWD,
                            "PWD\r\n",
                            FtpSocketDataProvider::PRE_TYPE,
                            "257 \"\"\r\n");

  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

}  // namespace net
