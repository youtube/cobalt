// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
#define NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_

// The UpdateFtpServerTypeHistograms function collects statistics related
// to the types of FTP servers that our users are encountering.  The
// information will help us decide for what servers we can safely drop support.
// The parsing code for each type of server is very complex, which poses a
// security risk. In fact, in current shape it's a disaster waiting to happen.

namespace net {

enum FtpServerType {
  // Record cases in which we couldn't parse the server's response. That means
  // a server type we don't recognize, a security attack (when what we're
  // connecting to isn't an FTP server), or a broken server.
  SERVER_UNKNOWN = 0,

  // The types below are recognized by ParseFTPList code from Mozilla. If we hit
  // one of these, it means that our new LIST parser failed for that server.
  SERVER_MOZ_LSL = 1,   // Server using /bin/ls -l and variants.
  SERVER_MOZ_DLS = 2,   // Server using /bin/dls.
  SERVER_MOZ_EPLF = 3,  // Server using EPLF format.
  SERVER_MOZ_DOS = 4,   // WinNT server configured for old style listing.
  SERVER_MOZ_VMS = 5,   // VMS (including variants).
  SERVER_MOZ_CMS = 6,   // IBM VM/CMS, VM/ESA, z/VM formats.
  SERVER_MOZ_OS2 = 7,   // OS/2 FTP Server.
  SERVER_MOZ_W16 = 8,   // win16 hosts: SuperTCP or NetManage Chameleon.

  // The types below are recognized by our new LIST parser. If we hit one of
  // these, it means that it's working quite well.
  SERVER_LS = 9,        // Server using /bin/ls -l listing style.
  SERVER_WINDOWS = 10,  // Server using Windows listing style.
  SERVER_VMS = 11,      // Server using VMS listing style.
  SERVER_NETWARE = 12,  // Server using Netware listing style.

  NUM_OF_SERVER_TYPES
};

void UpdateFtpServerTypeHistograms(FtpServerType type);

}  // namespace net

#endif  // NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
