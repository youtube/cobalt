// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/winsock_util.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Prevent the compiler from optimizing away the arguments so they appear
// nicely on the stack in crash dumps.
#pragma warning (disable: 4748)
#pragma optimize( "", off )

// Pass the important values as function arguments so that they are available
// in crash dumps.
void CheckEventWait(WSAEVENT hEvent, DWORD wait_rv, DWORD expected) {
  if (wait_rv != expected) {
    DWORD err = ERROR_SUCCESS;
    if (wait_rv == WAIT_FAILED)
      err = GetLastError();
    CHECK(false);  // Crash.
  }
}

#pragma optimize( "", on )
#pragma warning (default: 4748)

}  // namespace

void AssertEventNotSignaled(WSAEVENT hEvent) {
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  CheckEventWait(hEvent, wait_rv, WAIT_TIMEOUT);
}

bool ResetEventIfSignaled(WSAEVENT hEvent) {
  // TODO(wtc): Remove the CHECKs after enough testing.
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  if (wait_rv == WAIT_TIMEOUT)
    return false;  // The event object is not signaled.
  CheckEventWait(hEvent, wait_rv, WAIT_OBJECT_0);
  BOOL ok = WSAResetEvent(hEvent);
  CHECK(ok);
  return true;
}

// Map winsock error to Chromium error.
int MapWinsockError(int os_error) {
  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case WSAEACCES:
      return ERR_ACCESS_DENIED;
    case WSAENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case WSAETIMEDOUT:
      return ERR_TIMED_OUT;
    case WSAECONNRESET:
    case WSAENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case WSAECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case WSAECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case WSA_IO_INCOMPLETE:
    case WSAEDISCON:
      // WSAEDISCON is returned by WSARecv or WSARecvFrom for message-oriented
      // sockets (where a return value of zero means a zero-byte message) to
      // indicate graceful connection shutdown.  We should not ever see this
      // error code for TCP sockets, which are byte stream oriented.
      LOG(DFATAL) << "Unexpected error " << os_error
                  << " mapped to net::ERR_UNEXPECTED";
      return ERR_UNEXPECTED;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case WSAENOTCONN:
      return ERR_SOCKET_NOT_CONNECTED;
    case WSAEAFNOSUUPORT:
      return ERR_ADDRESS_UNREACHABLE;
    case ERROR_SUCCESS:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << os_error
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

}  // namespace net
