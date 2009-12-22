// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_TRANSACTION_H_
#define NET_FTP_FTP_NETWORK_TRANSACTION_H_

#include <string>
#include <queue>
#include <utility>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/ftp/ftp_ctrl_response_buffer.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction.h"

namespace net {

class ClientSocket;
class ClientSocketFactory;
class FtpNetworkSession;

class FtpNetworkTransaction : public FtpTransaction {
 public:
  FtpNetworkTransaction(FtpNetworkSession* session,
                        ClientSocketFactory* socket_factory);
  virtual ~FtpNetworkTransaction();

  // FtpTransaction methods:
  virtual int Start(const FtpRequestInfo* request_info,
                    CompletionCallback* callback,
                    LoadLog* load_log);
  virtual int Stop(int error);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const FtpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  enum Command {
    COMMAND_NONE,
    COMMAND_USER,
    COMMAND_PASS,
    COMMAND_ACCT,
    COMMAND_SYST,
    COMMAND_TYPE,
    COMMAND_PASV,
    COMMAND_PWD,
    COMMAND_SIZE,
    COMMAND_RETR,
    COMMAND_CWD,
    COMMAND_MLSD,
    COMMAND_LIST,
    COMMAND_MDTM,
    COMMAND_QUIT
  };

  enum ErrorClass {
    // The requested action was initiated. The client should expect another
    // reply before issuing the next command.
    ERROR_CLASS_INITIATED,

    // The requested action has been successfully completed.
    ERROR_CLASS_OK,

    // The command has been accepted, but to complete the operation, more
    // information must be sent by the client.
    ERROR_CLASS_INFO_NEEDED,

    // The command was not accepted and the requested action did not take place.
    // This condition is temporary, and the client is encouraged to restart the
    // command sequence.
    ERROR_CLASS_TRANSIENT_ERROR,

    // The command was not accepted and the requested action did not take place.
    // This condition is rather permanent, and the client is discouraged from
    // repeating the exact request.
    ERROR_CLASS_PERMANENT_ERROR,
  };

  // Major categories of remote system types, as returned by SYST command.
  enum SystemType {
    SYSTEM_TYPE_UNKNOWN,
    SYSTEM_TYPE_UNIX,
    SYSTEM_TYPE_WINDOWS,
    SYSTEM_TYPE_OS2,
    SYSTEM_TYPE_VMS,
  };

  // Resets the members of the transaction so it can be restarted.
  void ResetStateForRestart();

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Executes correct ProcessResponse + command_name function based on last
  // issued command. Returns error code.
  int ProcessCtrlResponse();

  int SendFtpCommand(const std::string& command, Command cmd);

  // Return the error class for given response code. You should validate the
  // code to be in range 100-599.
  static ErrorClass GetErrorClass(int response_code);

  // Returns request path suitable to be included in an FTP command. If the path
  // will be used as a directory, |is_directory| should be true.
  std::string GetRequestPathForFtpCommand(bool is_directory) const;

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoCtrlInit();
  int DoCtrlInitComplete(int result);
  int DoCtrlResolveHost();
  int DoCtrlResolveHostComplete(int result);
  int DoCtrlConnect();
  int DoCtrlConnectComplete(int result);
  int DoCtrlRead();
  int DoCtrlReadComplete(int result);
  int DoCtrlWrite();
  int DoCtrlWriteComplete(int result);
  int DoCtrlWriteUSER();
  int ProcessResponseUSER(const FtpCtrlResponse& response);
  int DoCtrlWritePASS();
  int ProcessResponsePASS(const FtpCtrlResponse& response);
  int DoCtrlWriteACCT();
  int ProcessResponseACCT(const FtpCtrlResponse& response);
  int DoCtrlWriteSYST();
  int ProcessResponseSYST(const FtpCtrlResponse& response);
  int DoCtrlWritePWD();
  int ProcessResponsePWD(const FtpCtrlResponse& response);
  int DoCtrlWriteTYPE();
  int ProcessResponseTYPE(const FtpCtrlResponse& response);
  int DoCtrlWritePASV();
  int ProcessResponsePASV(const FtpCtrlResponse& response);
  int DoCtrlWriteRETR();
  int ProcessResponseRETR(const FtpCtrlResponse& response);
  int DoCtrlWriteSIZE();
  int ProcessResponseSIZE(const FtpCtrlResponse& response);
  int DoCtrlWriteCWD();
  int ProcessResponseCWD(const FtpCtrlResponse& response);
  int DoCtrlWriteMLSD();
  int ProcessResponseMLSD(const FtpCtrlResponse& response);
  int DoCtrlWriteLIST();
  int ProcessResponseLIST(const FtpCtrlResponse& response);
  int DoCtrlWriteMDTM();
  int ProcessResponseMDTM(const FtpCtrlResponse& response);
  int DoCtrlWriteQUIT();
  int ProcessResponseQUIT(const FtpCtrlResponse& response);

  int DoDataConnect();
  int DoDataConnectComplete(int result);
  int DoDataRead();
  int DoDataReadComplete(int result);

  void RecordDataConnectionError(int result);

  Command command_sent_;

  CompletionCallbackImpl<FtpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<FtpNetworkSession> session_;

  scoped_refptr<LoadLog> load_log_;
  const FtpRequestInfo* request_;
  FtpResponseInfo response_;

  // Cancels the outstanding request on destruction.
  SingleRequestHostResolver resolver_;
  AddressList addresses_;

  // User buffer passed to the Read method for control socket.
  scoped_refptr<IOBuffer> read_ctrl_buf_;

  scoped_ptr<FtpCtrlResponseBuffer> ctrl_response_buffer_;

  scoped_refptr<IOBuffer> read_data_buf_;
  int read_data_buf_len_;
  int file_data_len_;

  // Buffer holding the command line to be written to the control socket.
  scoped_refptr<IOBufferWithSize> write_command_buf_;

  // Buffer passed to the Write method of control socket. It actually writes
  // to the write_command_buf_ at correct offset.
  scoped_refptr<DrainableIOBuffer> write_buf_;

  int last_error_;

  SystemType system_type_;

  // We get username and password as wstrings in RestartWithAuth, so they are
  // also kept as wstrings here.
  std::wstring username_;
  std::wstring password_;

  // Current directory on the remote server, as returned by last PWD command,
  // with any trailing slash removed.
  std::string current_remote_directory_;

  bool retr_failed_;

  int data_connection_port_;

  ClientSocketFactory* socket_factory_;

  scoped_ptr<ClientSocket> ctrl_socket_;
  scoped_ptr<ClientSocket> data_socket_;

  enum State {
    // Control connection states:
    STATE_CTRL_INIT,
    STATE_CTRL_INIT_COMPLETE,
    STATE_CTRL_RESOLVE_HOST,
    STATE_CTRL_RESOLVE_HOST_COMPLETE,
    STATE_CTRL_CONNECT,
    STATE_CTRL_CONNECT_COMPLETE,
    STATE_CTRL_READ,
    STATE_CTRL_READ_COMPLETE,
    STATE_CTRL_WRITE,
    STATE_CTRL_WRITE_COMPLETE,
    STATE_CTRL_WRITE_USER,
    STATE_CTRL_WRITE_PASS,
    STATE_CTRL_WRITE_ACCT,
    STATE_CTRL_WRITE_SYST,
    STATE_CTRL_WRITE_TYPE,
    STATE_CTRL_WRITE_PASV,
    STATE_CTRL_WRITE_PWD,
    STATE_CTRL_WRITE_RETR,
    STATE_CTRL_WRITE_SIZE,
    STATE_CTRL_WRITE_CWD,
    STATE_CTRL_WRITE_MLSD,
    STATE_CTRL_WRITE_LIST,
    STATE_CTRL_WRITE_MDTM,
    STATE_CTRL_WRITE_QUIT,
    // Data connection states:
    STATE_DATA_CONNECT,
    STATE_DATA_CONNECT_COMPLETE,
    STATE_DATA_READ,
    STATE_DATA_READ_COMPLETE,
    STATE_NONE
  };
  State next_state_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_TRANSACTION_H_
