// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some protocol structures for use with Spdy.

#ifndef NET_SPDY_SPDY_PROTOCOL_H_
#define NET_SPDY_SPDY_PROTOCOL_H_
#pragma once

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "net/spdy/spdy_bitmasks.h"

//  Data Frame Format
//  +----------------------------------+
//  |0|       Stream-ID (31bits)       |
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |               Data               |
//  +----------------------------------+
//
//  Control Frame Format
//  +----------------------------------+
//  |1| Version(15bits) | Type(16bits) |
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |               Data               |
//  +----------------------------------+
//
//  Control Frame: SYN_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000001|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 12
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |X|Associated-To-Stream-ID (31bits)|
//  +----------------------------------+
//  |Pri| unused      | Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: SYN_REPLY
//  +----------------------------------+
//  |1|000000000000001|0000000000000010|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 8
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  | unused (16 bits)| Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: RST_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000011|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 4
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |        Status code (32 bits)     |
//  +----------------------------------+
//
//  Control Frame: SETTINGS
//  +----------------------------------+
//  |1|000000000000001|0000000000000100|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |        # of entries (32)         |
//  +----------------------------------+
//
//  Control Frame: NOOP
//  +----------------------------------+
//  |1|000000000000001|0000000000000101|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 0
//  +----------------------------------+
//
//  Control Frame: PING
//  +----------------------------------+
//  |1|000000000000001|0000000000000110|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 4
//  +----------------------------------+
//  |        Unique id (32 bits)       |
//  +----------------------------------+
//
//  Control Frame: GOAWAY
//  +----------------------------------+
//  |1|000000000000001|0000000000000111|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 4
//  +----------------------------------+
//  |X|  Last-accepted-stream-id       |
//  +----------------------------------+
//
//  Control Frame: HEADERS
//  +----------------------------------+
//  |1|000000000000001|0000000000001000|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | >= 8
//  +----------------------------------+
//  |X|      Stream-ID (31 bits)       |
//  +----------------------------------+
//  | unused (16 bits)| Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: WINDOW_UPDATE
//  +----------------------------------+
//  |1|000000000000001|0000000000001001|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 8
//  +----------------------------------+
//  |X|      Stream-ID (31 bits)       |
//  +----------------------------------+
//  |   Delta-Window-Size (32 bits)    |
//  +----------------------------------+
//
//  Control Frame: CREDENTIAL
//  +----------------------------------+
//  |1|000000000000001|0000000000001010|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | >= 12
//  +----------------------------------+
//  |  Slot (16 bits) |                |
//  +-----------------+                |
//  |      Proof Length (32 bits)      |
//  +----------------------------------+
//  |               Proof              |
//  +----------------------------------+ <+
//  |   Certificate Length (32 bits)   |  |
//  +----------------------------------+  | Repeated until end of frame
//  |            Certificate           |  |
//  +----------------------------------+ <+
//

namespace net {

// Initial window size for a Spdy stream
const int32 kSpdyStreamInitialWindowSize = 64 * 1024;  // 64 KBytes

// Maximum window size for a Spdy stream
const int32 kSpdyStreamMaximumWindowSize = 0x7FFFFFFF;  // Max signed 32bit int

// SPDY 2 dictionary.
// This is just a hacked dictionary to use for shrinking HTTP-like headers.
const char kV2Dictionary[] =
  "optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-"
  "languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi"
  "f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser"
  "-agent10010120020120220320420520630030130230330430530630740040140240340440"
  "5406407408409410411412413414415416417500501502503504505accept-rangesageeta"
  "glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic"
  "ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran"
  "sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati"
  "oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo"
  "ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe"
  "pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic"
  "ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1"
  ".1statusversionurl";
const int kV2DictionarySize = arraysize(kV2Dictionary);

// SPDY 3 dictionary.
const char kV3Dictionary[] = {
  0x00, 0x00, 0x00, 0x07, 0x6f, 0x70, 0x74, 0x69,  // ....opti
  0x6f, 0x6e, 0x73, 0x00, 0x00, 0x00, 0x04, 0x68,  // ons....h
  0x65, 0x61, 0x64, 0x00, 0x00, 0x00, 0x04, 0x70,  // ead....p
  0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x03, 0x70,  // ost....p
  0x75, 0x74, 0x00, 0x00, 0x00, 0x06, 0x64, 0x65,  // ut....de
  0x6c, 0x65, 0x74, 0x65, 0x00, 0x00, 0x00, 0x05,  // lete....
  0x74, 0x72, 0x61, 0x63, 0x65, 0x00, 0x00, 0x00,  // trace...
  0x06, 0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x00,  // .accept.
  0x00, 0x00, 0x0e, 0x61, 0x63, 0x63, 0x65, 0x70,  // ...accep
  0x74, 0x2d, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,  // t-charse
  0x74, 0x00, 0x00, 0x00, 0x0f, 0x61, 0x63, 0x63,  // t....acc
  0x65, 0x70, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,  // ept-enco
  0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x0f,  // ding....
  0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x2d, 0x6c,  // accept-l
  0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65, 0x00,  // anguage.
  0x00, 0x00, 0x0d, 0x61, 0x63, 0x63, 0x65, 0x70,  // ...accep
  0x74, 0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x73,  // t-ranges
  0x00, 0x00, 0x00, 0x03, 0x61, 0x67, 0x65, 0x00,  // ....age.
  0x00, 0x00, 0x05, 0x61, 0x6c, 0x6c, 0x6f, 0x77,  // ...allow
  0x00, 0x00, 0x00, 0x0d, 0x61, 0x75, 0x74, 0x68,  // ....auth
  0x6f, 0x72, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f,  // orizatio
  0x6e, 0x00, 0x00, 0x00, 0x0d, 0x63, 0x61, 0x63,  // n....cac
  0x68, 0x65, 0x2d, 0x63, 0x6f, 0x6e, 0x74, 0x72,  // he-contr
  0x6f, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x6f,  // ol....co
  0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e,  // nnection
  0x00, 0x00, 0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74,  // ....cont
  0x65, 0x6e, 0x74, 0x2d, 0x62, 0x61, 0x73, 0x65,  // ent-base
  0x00, 0x00, 0x00, 0x10, 0x63, 0x6f, 0x6e, 0x74,  // ....cont
  0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,  // ent-enco
  0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x10,  // ding....
  0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d,  // content-
  0x6c, 0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65,  // language
  0x00, 0x00, 0x00, 0x0e, 0x63, 0x6f, 0x6e, 0x74,  // ....cont
  0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67,  // ent-leng
  0x74, 0x68, 0x00, 0x00, 0x00, 0x10, 0x63, 0x6f,  // th....co
  0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x6f,  // ntent-lo
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00,  // cation..
  0x00, 0x0b, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,  // ..conten
  0x74, 0x2d, 0x6d, 0x64, 0x35, 0x00, 0x00, 0x00,  // t-md5...
  0x0d, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74,  // .content
  0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00,  // -range..
  0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,  // ..conten
  0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x00, 0x00,  // t-type..
  0x00, 0x04, 0x64, 0x61, 0x74, 0x65, 0x00, 0x00,  // ..date..
  0x00, 0x04, 0x65, 0x74, 0x61, 0x67, 0x00, 0x00,  // ..etag..
  0x00, 0x06, 0x65, 0x78, 0x70, 0x65, 0x63, 0x74,  // ..expect
  0x00, 0x00, 0x00, 0x07, 0x65, 0x78, 0x70, 0x69,  // ....expi
  0x72, 0x65, 0x73, 0x00, 0x00, 0x00, 0x04, 0x66,  // res....f
  0x72, 0x6f, 0x6d, 0x00, 0x00, 0x00, 0x04, 0x68,  // rom....h
  0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x08, 0x69,  // ost....i
  0x66, 0x2d, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x00,  // f-match.
  0x00, 0x00, 0x11, 0x69, 0x66, 0x2d, 0x6d, 0x6f,  // ...if-mo
  0x64, 0x69, 0x66, 0x69, 0x65, 0x64, 0x2d, 0x73,  // dified-s
  0x69, 0x6e, 0x63, 0x65, 0x00, 0x00, 0x00, 0x0d,  // ince....
  0x69, 0x66, 0x2d, 0x6e, 0x6f, 0x6e, 0x65, 0x2d,  // if-none-
  0x6d, 0x61, 0x74, 0x63, 0x68, 0x00, 0x00, 0x00,  // match...
  0x08, 0x69, 0x66, 0x2d, 0x72, 0x61, 0x6e, 0x67,  // .if-rang
  0x65, 0x00, 0x00, 0x00, 0x13, 0x69, 0x66, 0x2d,  // e....if-
  0x75, 0x6e, 0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69,  // unmodifi
  0x65, 0x64, 0x2d, 0x73, 0x69, 0x6e, 0x63, 0x65,  // ed-since
  0x00, 0x00, 0x00, 0x0d, 0x6c, 0x61, 0x73, 0x74,  // ....last
  0x2d, 0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69, 0x65,  // -modifie
  0x64, 0x00, 0x00, 0x00, 0x08, 0x6c, 0x6f, 0x63,  // d....loc
  0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00,  // ation...
  0x0c, 0x6d, 0x61, 0x78, 0x2d, 0x66, 0x6f, 0x72,  // .max-for
  0x77, 0x61, 0x72, 0x64, 0x73, 0x00, 0x00, 0x00,  // wards...
  0x06, 0x70, 0x72, 0x61, 0x67, 0x6d, 0x61, 0x00,  // .pragma.
  0x00, 0x00, 0x12, 0x70, 0x72, 0x6f, 0x78, 0x79,  // ...proxy
  0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e, 0x74,  // -authent
  0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00,  // icate...
  0x13, 0x70, 0x72, 0x6f, 0x78, 0x79, 0x2d, 0x61,  // .proxy-a
  0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x7a, 0x61,  // uthoriza
  0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x05,  // tion....
  0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00, 0x00,  // range...
  0x07, 0x72, 0x65, 0x66, 0x65, 0x72, 0x65, 0x72,  // .referer
  0x00, 0x00, 0x00, 0x0b, 0x72, 0x65, 0x74, 0x72,  // ....retr
  0x79, 0x2d, 0x61, 0x66, 0x74, 0x65, 0x72, 0x00,  // y-after.
  0x00, 0x00, 0x06, 0x73, 0x65, 0x72, 0x76, 0x65,  // ...serve
  0x72, 0x00, 0x00, 0x00, 0x02, 0x74, 0x65, 0x00,  // r....te.
  0x00, 0x00, 0x07, 0x74, 0x72, 0x61, 0x69, 0x6c,  // ...trail
  0x65, 0x72, 0x00, 0x00, 0x00, 0x11, 0x74, 0x72,  // er....tr
  0x61, 0x6e, 0x73, 0x66, 0x65, 0x72, 0x2d, 0x65,  // ansfer-e
  0x6e, 0x63, 0x6f, 0x64, 0x69, 0x6e, 0x67, 0x00,  // ncoding.
  0x00, 0x00, 0x07, 0x75, 0x70, 0x67, 0x72, 0x61,  // ...upgra
  0x64, 0x65, 0x00, 0x00, 0x00, 0x0a, 0x75, 0x73,  // de....us
  0x65, 0x72, 0x2d, 0x61, 0x67, 0x65, 0x6e, 0x74,  // er-agent
  0x00, 0x00, 0x00, 0x04, 0x76, 0x61, 0x72, 0x79,  // ....vary
  0x00, 0x00, 0x00, 0x03, 0x76, 0x69, 0x61, 0x00,  // ....via.
  0x00, 0x00, 0x07, 0x77, 0x61, 0x72, 0x6e, 0x69,  // ...warni
  0x6e, 0x67, 0x00, 0x00, 0x00, 0x10, 0x77, 0x77,  // ng....ww
  0x77, 0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e,  // w-authen
  0x74, 0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00,  // ticate..
  0x00, 0x06, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64,  // ..method
  0x00, 0x00, 0x00, 0x03, 0x67, 0x65, 0x74, 0x00,  // ....get.
  0x00, 0x00, 0x06, 0x73, 0x74, 0x61, 0x74, 0x75,  // ...statu
  0x73, 0x00, 0x00, 0x00, 0x06, 0x32, 0x30, 0x30,  // s....200
  0x20, 0x4f, 0x4b, 0x00, 0x00, 0x00, 0x07, 0x76,  // .OK....v
  0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x00,  // ersion..
  0x00, 0x08, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31,  // ..HTTP.1
  0x2e, 0x31, 0x00, 0x00, 0x00, 0x03, 0x75, 0x72,  // .1....ur
  0x6c, 0x00, 0x00, 0x00, 0x06, 0x70, 0x75, 0x62,  // l....pub
  0x6c, 0x69, 0x63, 0x00, 0x00, 0x00, 0x0a, 0x73,  // lic....s
  0x65, 0x74, 0x2d, 0x63, 0x6f, 0x6f, 0x6b, 0x69,  // et-cooki
  0x65, 0x00, 0x00, 0x00, 0x0a, 0x6b, 0x65, 0x65,  // e....kee
  0x70, 0x2d, 0x61, 0x6c, 0x69, 0x76, 0x65, 0x00,  // p-alive.
  0x00, 0x00, 0x06, 0x6f, 0x72, 0x69, 0x67, 0x69,  // ...origi
  0x6e, 0x31, 0x30, 0x30, 0x31, 0x30, 0x31, 0x32,  // n1001012
  0x30, 0x31, 0x32, 0x30, 0x32, 0x32, 0x30, 0x35,  // 01202205
  0x32, 0x30, 0x36, 0x33, 0x30, 0x30, 0x33, 0x30,  // 20630030
  0x32, 0x33, 0x30, 0x33, 0x33, 0x30, 0x34, 0x33,  // 23033043
  0x30, 0x35, 0x33, 0x30, 0x36, 0x33, 0x30, 0x37,  // 05306307
  0x34, 0x30, 0x32, 0x34, 0x30, 0x35, 0x34, 0x30,  // 40240540
  0x36, 0x34, 0x30, 0x37, 0x34, 0x30, 0x38, 0x34,  // 64074084
  0x30, 0x39, 0x34, 0x31, 0x30, 0x34, 0x31, 0x31,  // 09410411
  0x34, 0x31, 0x32, 0x34, 0x31, 0x33, 0x34, 0x31,  // 41241341
  0x34, 0x34, 0x31, 0x35, 0x34, 0x31, 0x36, 0x34,  // 44154164
  0x31, 0x37, 0x35, 0x30, 0x32, 0x35, 0x30, 0x34,  // 17502504
  0x35, 0x30, 0x35, 0x32, 0x30, 0x33, 0x20, 0x4e,  // 505203.N
  0x6f, 0x6e, 0x2d, 0x41, 0x75, 0x74, 0x68, 0x6f,  // on-Autho
  0x72, 0x69, 0x74, 0x61, 0x74, 0x69, 0x76, 0x65,  // ritative
  0x20, 0x49, 0x6e, 0x66, 0x6f, 0x72, 0x6d, 0x61,  // .Informa
  0x74, 0x69, 0x6f, 0x6e, 0x32, 0x30, 0x34, 0x20,  // tion204.
  0x4e, 0x6f, 0x20, 0x43, 0x6f, 0x6e, 0x74, 0x65,  // No.Conte
  0x6e, 0x74, 0x33, 0x30, 0x31, 0x20, 0x4d, 0x6f,  // nt301.Mo
  0x76, 0x65, 0x64, 0x20, 0x50, 0x65, 0x72, 0x6d,  // ved.Perm
  0x61, 0x6e, 0x65, 0x6e, 0x74, 0x6c, 0x79, 0x34,  // anently4
  0x30, 0x30, 0x20, 0x42, 0x61, 0x64, 0x20, 0x52,  // 00.Bad.R
  0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x34, 0x30,  // equest40
  0x31, 0x20, 0x55, 0x6e, 0x61, 0x75, 0x74, 0x68,  // 1.Unauth
  0x6f, 0x72, 0x69, 0x7a, 0x65, 0x64, 0x34, 0x30,  // orized40
  0x33, 0x20, 0x46, 0x6f, 0x72, 0x62, 0x69, 0x64,  // 3.Forbid
  0x64, 0x65, 0x6e, 0x34, 0x30, 0x34, 0x20, 0x4e,  // den404.N
  0x6f, 0x74, 0x20, 0x46, 0x6f, 0x75, 0x6e, 0x64,  // ot.Found
  0x35, 0x30, 0x30, 0x20, 0x49, 0x6e, 0x74, 0x65,  // 500.Inte
  0x72, 0x6e, 0x61, 0x6c, 0x20, 0x53, 0x65, 0x72,  // rnal.Ser
  0x76, 0x65, 0x72, 0x20, 0x45, 0x72, 0x72, 0x6f,  // ver.Erro
  0x72, 0x35, 0x30, 0x31, 0x20, 0x4e, 0x6f, 0x74,  // r501.Not
  0x20, 0x49, 0x6d, 0x70, 0x6c, 0x65, 0x6d, 0x65,  // .Impleme
  0x6e, 0x74, 0x65, 0x64, 0x35, 0x30, 0x33, 0x20,  // nted503.
  0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20,  // Service.
  0x55, 0x6e, 0x61, 0x76, 0x61, 0x69, 0x6c, 0x61,  // Unavaila
  0x62, 0x6c, 0x65, 0x4a, 0x61, 0x6e, 0x20, 0x46,  // bleJan.F
  0x65, 0x62, 0x20, 0x4d, 0x61, 0x72, 0x20, 0x41,  // eb.Mar.A
  0x70, 0x72, 0x20, 0x4d, 0x61, 0x79, 0x20, 0x4a,  // pr.May.J
  0x75, 0x6e, 0x20, 0x4a, 0x75, 0x6c, 0x20, 0x41,  // un.Jul.A
  0x75, 0x67, 0x20, 0x53, 0x65, 0x70, 0x74, 0x20,  // ug.Sept.
  0x4f, 0x63, 0x74, 0x20, 0x4e, 0x6f, 0x76, 0x20,  // Oct.Nov.
  0x44, 0x65, 0x63, 0x20, 0x30, 0x30, 0x3a, 0x30,  // Dec.00.0
  0x30, 0x3a, 0x30, 0x30, 0x20, 0x4d, 0x6f, 0x6e,  // 0.00.Mon
  0x2c, 0x20, 0x54, 0x75, 0x65, 0x2c, 0x20, 0x57,  // ..Tue..W
  0x65, 0x64, 0x2c, 0x20, 0x54, 0x68, 0x75, 0x2c,  // ed..Thu.
  0x20, 0x46, 0x72, 0x69, 0x2c, 0x20, 0x53, 0x61,  // .Fri..Sa
  0x74, 0x2c, 0x20, 0x53, 0x75, 0x6e, 0x2c, 0x20,  // t..Sun..
  0x47, 0x4d, 0x54, 0x63, 0x68, 0x75, 0x6e, 0x6b,  // GMTchunk
  0x65, 0x64, 0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f,  // ed.text.
  0x68, 0x74, 0x6d, 0x6c, 0x2c, 0x69, 0x6d, 0x61,  // html.ima
  0x67, 0x65, 0x2f, 0x70, 0x6e, 0x67, 0x2c, 0x69,  // ge.png.i
  0x6d, 0x61, 0x67, 0x65, 0x2f, 0x6a, 0x70, 0x67,  // mage.jpg
  0x2c, 0x69, 0x6d, 0x61, 0x67, 0x65, 0x2f, 0x67,  // .image.g
  0x69, 0x66, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69,  // if.appli
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,  // cation.x
  0x6d, 0x6c, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69,  // ml.appli
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,  // cation.x
  0x68, 0x74, 0x6d, 0x6c, 0x2b, 0x78, 0x6d, 0x6c,  // html.xml
  0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c,  // .text.pl
  0x61, 0x69, 0x6e, 0x2c, 0x74, 0x65, 0x78, 0x74,  // ain.text
  0x2f, 0x6a, 0x61, 0x76, 0x61, 0x73, 0x63, 0x72,  // .javascr
  0x69, 0x70, 0x74, 0x2c, 0x70, 0x75, 0x62, 0x6c,  // ipt.publ
  0x69, 0x63, 0x70, 0x72, 0x69, 0x76, 0x61, 0x74,  // icprivat
  0x65, 0x6d, 0x61, 0x78, 0x2d, 0x61, 0x67, 0x65,  // emax-age
  0x3d, 0x67, 0x7a, 0x69, 0x70, 0x2c, 0x64, 0x65,  // .gzip.de
  0x66, 0x6c, 0x61, 0x74, 0x65, 0x2c, 0x73, 0x64,  // flate.sd
  0x63, 0x68, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,  // chcharse
  0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38, 0x63,  // t.utf-8c
  0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x69,  // harset.i
  0x73, 0x6f, 0x2d, 0x38, 0x38, 0x35, 0x39, 0x2d,  // so-8859-
  0x31, 0x2c, 0x75, 0x74, 0x66, 0x2d, 0x2c, 0x2a,  // 1.utf-..
  0x2c, 0x65, 0x6e, 0x71, 0x3d, 0x30, 0x2e         // .enq.0.
};
const int kV3DictionarySize = arraysize(kV3Dictionary);

// Note: all protocol data structures are on-the-wire format.  That means that
//       data is stored in network-normalized order.  Readers must use the
//       accessors provided or call ntohX() functions.

// Types of Spdy Control Frames.
enum SpdyControlType {
  SYN_STREAM = 1,
  SYN_REPLY,
  RST_STREAM,
  SETTINGS,
  NOOP,  // Because it is valid in SPDY/2, kept for identifiability/enum order.
  PING,
  GOAWAY,
  HEADERS,
  WINDOW_UPDATE,
  CREDENTIAL,
  NUM_CONTROL_FRAME_TYPES
};

// Flags on data packets.
enum SpdyDataFlags {
  DATA_FLAG_NONE = 0,
  DATA_FLAG_FIN = 1,
};

// Flags on control packets
enum SpdyControlFlags {
  CONTROL_FLAG_NONE = 0,
  CONTROL_FLAG_FIN = 1,
  CONTROL_FLAG_UNIDIRECTIONAL = 2
};

// Flags on the SETTINGS control frame.
enum SpdySettingsControlFlags {
  SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS = 0x1
};

// Flags for settings within a SETTINGS frame.
enum SpdySettingsFlags {
  SETTINGS_FLAG_NONE = 0x0,
  SETTINGS_FLAG_PLEASE_PERSIST = 0x1,
  SETTINGS_FLAG_PERSISTED = 0x2
};

// List of known settings.
enum SpdySettingsIds {
  SETTINGS_UPLOAD_BANDWIDTH = 0x1,
  SETTINGS_DOWNLOAD_BANDWIDTH = 0x2,
  // Network round trip time in milliseconds.
  SETTINGS_ROUND_TRIP_TIME = 0x3,
  SETTINGS_MAX_CONCURRENT_STREAMS = 0x4,
  // TCP congestion window in packets.
  SETTINGS_CURRENT_CWND = 0x5,
  // Downstream byte retransmission rate in percentage.
  SETTINGS_DOWNLOAD_RETRANS_RATE = 0x6,
  // Initial window size in bytes
  SETTINGS_INITIAL_WINDOW_SIZE = 0x7
};

// Status codes, as used in control frames (primarily RST_STREAM).
// TODO(hkhalil): Rename to SpdyRstStreamStatus
enum SpdyStatusCodes {
  INVALID = 0,
  PROTOCOL_ERROR = 1,
  INVALID_STREAM = 2,
  REFUSED_STREAM = 3,
  UNSUPPORTED_VERSION = 4,
  CANCEL = 5,
  INTERNAL_ERROR = 6,
  FLOW_CONTROL_ERROR = 7,
  STREAM_IN_USE = 8,
  STREAM_ALREADY_CLOSED = 9,
  INVALID_CREDENTIALS = 10,
  FRAME_TOO_LARGE = 11,
  NUM_STATUS_CODES = 12
};

enum SpdyGoAwayStatus {
  GOAWAY_INVALID = -1,
  GOAWAY_OK = 0,
  GOAWAY_PROTOCOL_ERROR = 1,
  GOAWAY_INTERNAL_ERROR = 2,
  GOAWAY_NUM_STATUS_CODES = 3
};

// A SPDY stream id is a 31 bit entity.
typedef uint32 SpdyStreamId;

// A SPDY priority is a number between 0 and 7 (inclusive).
// SPDY priority range is version-dependant. For SPDY 2 and below, priority is a
// number between 0 and 3.
typedef uint8 SpdyPriority;

// -------------------------------------------------------------------------
// These structures mirror the protocol structure definitions.

// For the control data structures, we pack so that sizes match the
// protocol over-the-wire sizes.
#pragma pack(push)
#pragma pack(1)

// A special structure for the 8 bit flags and 24 bit length fields.
union FlagsAndLength {
  uint8 flags_[4];  // 8 bits
  uint32 length_;   // 24 bits
};

// The basic SPDY Frame structure.
struct SpdyFrameBlock {
  union {
    struct {
      uint16 version_;
      uint16 type_;
    } control_;
    struct {
      SpdyStreamId stream_id_;
    } data_;
  };
  FlagsAndLength flags_length_;
};

// A SYN_STREAM Control Frame structure.
struct SpdySynStreamControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  SpdyStreamId associated_stream_id_;
  SpdyPriority priority_;
  uint8 credential_slot_;
};

// A SYN_REPLY Control Frame structure.
struct SpdySynReplyControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
};

// A RST_STREAM Control Frame structure.
struct SpdyRstStreamControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  uint32 status_;
};

// A SETTINGS Control Frame structure.
struct SpdySettingsControlFrameBlock : SpdyFrameBlock {
  uint32 num_entries_;
  // Variable data here.
};

// A PING Control Frame structure.
struct SpdyPingControlFrameBlock : SpdyFrameBlock {
  uint32 unique_id_;
};

// TODO(avd): remove this struct
// A CREDENTIAL Control Frame structure.
struct SpdyCredentialControlFrameBlock : SpdyFrameBlock {
  uint16 slot_;
  uint32 proof_len_;
  // Variable data here.
  // proof data
  // for each certificate: unit32 certificate_len + certificate_data[i]
};

// A GOAWAY Control Frame structure.
struct SpdyGoAwayControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId last_accepted_stream_id_;
  SpdyGoAwayStatus status_;
};

// A HEADERS Control Frame structure.
struct SpdyHeadersControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
};

// A WINDOW_UPDATE Control Frame structure
struct SpdyWindowUpdateControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  uint32 delta_window_size_;
};

#pragma pack(pop)

// -------------------------------------------------------------------------
// Wrapper classes for various SPDY frames.

// All Spdy Frame types derive from this SpdyFrame class.
class SpdyFrame {
 public:
  // Create a SpdyFrame for a given sized buffer.
  explicit SpdyFrame(size_t size) : frame_(NULL), owns_buffer_(true) {
    DCHECK_GE(size, sizeof(struct SpdyFrameBlock));
    char* buffer = new char[size];
    memset(buffer, 0, size);
    frame_ = reinterpret_cast<struct SpdyFrameBlock*>(buffer);
  }

  // Create a SpdyFrame using a pre-created buffer.
  // If |owns_buffer| is true, this class takes ownership of the buffer
  // and will delete it on cleanup.  The buffer must have been created using
  // new char[].
  // If |owns_buffer| is false, the caller retains ownership of the buffer and
  // is responsible for making sure the buffer outlives this frame.  In other
  // words, this class does NOT create a copy of the buffer.
  SpdyFrame(char* data, bool owns_buffer)
      : frame_(reinterpret_cast<struct SpdyFrameBlock*>(data)),
        owns_buffer_(owns_buffer) {
    DCHECK(frame_);
  }

  ~SpdyFrame() {
    if (owns_buffer_) {
      char* buffer = reinterpret_cast<char*>(frame_);
      delete [] buffer;
    }
    frame_ = NULL;
  }

  // Provides access to the frame bytes, which is a buffer containing
  // the frame packed as expected for sending over the wire.
  char* data() const { return reinterpret_cast<char*>(frame_); }

  uint8 flags() const { return frame_->flags_length_.flags_[0]; }
  void set_flags(uint8 flags) { frame_->flags_length_.flags_[0] = flags; }

  uint32 length() const {
    return ntohl(frame_->flags_length_.length_) & kLengthMask;
  }

  void set_length(uint32 length) {
    DCHECK_EQ(0u, (length & ~kLengthMask));
    length = htonl(length & kLengthMask);
    frame_->flags_length_.length_ = flags() | length;
  }

  bool is_control_frame() const {
    return (ntohs(frame_->control_.version_) & kControlFlagMask) ==
        kControlFlagMask;
  }

  // The size of the SpdyFrameBlock structure.
  // Every SpdyFrame* class has a static size() method for accessing
  // the size of the data structure which will be sent over the wire.
  // Note:  this is not the same as sizeof(SpdyFrame).
  enum { kHeaderSize = sizeof(struct SpdyFrameBlock) };

 protected:
  SpdyFrameBlock* frame_;

 private:
  bool owns_buffer_;
  DISALLOW_COPY_AND_ASSIGN(SpdyFrame);
};

// A Data Frame.
class SpdyDataFrame : public SpdyFrame {
 public:
  SpdyDataFrame() : SpdyFrame(size()) {}
  SpdyDataFrame(char* data, bool owns_buffer)
      : SpdyFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(frame_->data_.stream_id_) & kStreamIdMask;
  }

  // Note that setting the stream id sets the control bit to false.
  // As stream id should always be set, this means the control bit
  // should always be set correctly.
  void set_stream_id(SpdyStreamId id) {
    DCHECK_EQ(0u, (id & ~kStreamIdMask));
    frame_->data_.stream_id_ = htonl(id & kStreamIdMask);
  }

  // Returns the size of the SpdyFrameBlock structure.
  // Note: this is not the size of the SpdyDataFrame class.
  static size_t size() { return SpdyFrame::kHeaderSize; }

  const char* payload() const {
    return reinterpret_cast<const char*>(frame_) + size();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyDataFrame);
};

// A Control Frame.
class SpdyControlFrame : public SpdyFrame {
 public:
  explicit SpdyControlFrame(size_t size) : SpdyFrame(size) {}
  SpdyControlFrame(char* data, bool owns_buffer)
      : SpdyFrame(data, owns_buffer) {}

  // Callers can use this method to check if the frame appears to be a valid
  // frame.  Does not guarantee that there are no errors.
  bool AppearsToBeAValidControlFrame() const {
    // Right now we only check if the frame has an out-of-bounds type.
    uint16 type = ntohs(block()->control_.type_);
    // NOOP is not a 'valid' control frame in SPDY/3 and beyond.
    return type >= SYN_STREAM &&
        type < NUM_CONTROL_FRAME_TYPES &&
        (version() == 2 || type != NOOP);
  }

  uint16 version() const {
    const int kVersionMask = 0x7fff;
    return ntohs(block()->control_.version_) & kVersionMask;
  }

  void set_version(uint16 version) {
    const uint16 kControlBit = 0x80;
    DCHECK_EQ(0, version & kControlBit);
    mutable_block()->control_.version_ = kControlBit | htons(version);
  }

  SpdyControlType type() const {
    uint16 type = ntohs(block()->control_.type_);
    LOG_IF(DFATAL, type < SYN_STREAM || type >= NUM_CONTROL_FRAME_TYPES)
        << "Invalid control frame type " << type;
    return static_cast<SpdyControlType>(type);
  }

  void set_type(SpdyControlType type) {
    DCHECK(type >= SYN_STREAM && type < NUM_CONTROL_FRAME_TYPES);
    mutable_block()->control_.type_ = htons(type);
  }

  // Returns true if this control frame is of a type that has a header block,
  // otherwise it returns false.
  bool has_header_block() const {
    return type() == SYN_STREAM || type() == SYN_REPLY || type() == HEADERS;
  }

 private:
  const struct SpdyFrameBlock* block() const {
    return frame_;
  }
  struct SpdyFrameBlock* mutable_block() {
    return frame_;
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyControlFrame);
};

// A SYN_STREAM frame.
class SpdySynStreamControlFrame : public SpdyControlFrame {
 public:
  SpdySynStreamControlFrame() : SpdyControlFrame(size()) {}
  SpdySynStreamControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  SpdyStreamId associated_stream_id() const {
    return ntohl(block()->associated_stream_id_) & kStreamIdMask;
  }

  void set_associated_stream_id(SpdyStreamId id) {
    mutable_block()->associated_stream_id_ = htonl(id & kStreamIdMask);
  }

  SpdyPriority priority() const {
    if (version() < 3) {
      return (block()->priority_ & kSpdy2PriorityMask) >> 6;
    } else {
      return (block()->priority_ & kSpdy3PriorityMask) >> 5;
    }
  }

  uint8 credential_slot() const {
    if (version() < 3) {
      return 0;
    } else {
      return block()->credential_slot_;
    }
  }

  void set_credential_slot(uint8 credential_slot) {
    DCHECK(version() >= 3);
    mutable_block()->credential_slot_ = credential_slot;
  }

  // The number of bytes in the header block beyond the frame header length.
  int header_block_len() const {
    return length() - (size() - SpdyFrame::kHeaderSize);
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the SpdySynStreamControlFrameBlock structure.
  // Note: this is not the size of the SpdySynStreamControlFrame class.
  static size_t size() { return sizeof(SpdySynStreamControlFrameBlock); }

 private:
  const struct SpdySynStreamControlFrameBlock* block() const {
    return static_cast<SpdySynStreamControlFrameBlock*>(frame_);
  }
  struct SpdySynStreamControlFrameBlock* mutable_block() {
    return static_cast<SpdySynStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySynStreamControlFrame);
};

// A SYN_REPLY frame.
class SpdySynReplyControlFrame : public SpdyControlFrame {
 public:
  SpdySynReplyControlFrame() : SpdyControlFrame(size()) {}
  SpdySynReplyControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  int header_block_len() const {
    size_t header_block_len = length() - (size() - SpdyFrame::kHeaderSize);
    // SPDY 2 had 2 bytes of unused space preceeding the header block.
    if (version() < 3) {
      header_block_len -= 2;
    }
    return header_block_len;
  }

  const char* header_block() const {
    const char* header_block = reinterpret_cast<const char*>(block()) + size();
    // SPDY 2 had 2 bytes of unused space preceeding the header block.
    if (version() < 3) {
      header_block += 2;
    }
    return header_block;
  }

  // Returns the size of the SpdySynReplyControlFrameBlock structure.
  // Note: this is not the size of the SpdySynReplyControlFrame class.
  static size_t size() { return sizeof(SpdySynReplyControlFrameBlock); }

 private:
  const struct SpdySynReplyControlFrameBlock* block() const {
    return static_cast<SpdySynReplyControlFrameBlock*>(frame_);
  }
  struct SpdySynReplyControlFrameBlock* mutable_block() {
    return static_cast<SpdySynReplyControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySynReplyControlFrame);
};

// A RST_STREAM frame.
class SpdyRstStreamControlFrame : public SpdyControlFrame {
 public:
  SpdyRstStreamControlFrame() : SpdyControlFrame(size()) {}
  SpdyRstStreamControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  SpdyStatusCodes status() const {
    SpdyStatusCodes status =
        static_cast<SpdyStatusCodes>(ntohl(block()->status_));
    if (status < INVALID || status >= NUM_STATUS_CODES) {
      status = INVALID;
    }
    return status;
  }
  void set_status(SpdyStatusCodes status) {
    mutable_block()->status_ = htonl(static_cast<uint32>(status));
  }

  // Returns the size of the SpdyRstStreamControlFrameBlock structure.
  // Note: this is not the size of the SpdyRstStreamControlFrame class.
  static size_t size() { return sizeof(SpdyRstStreamControlFrameBlock); }

 private:
  const struct SpdyRstStreamControlFrameBlock* block() const {
    return static_cast<SpdyRstStreamControlFrameBlock*>(frame_);
  }
  struct SpdyRstStreamControlFrameBlock* mutable_block() {
    return static_cast<SpdyRstStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyRstStreamControlFrame);
};

class SpdySettingsControlFrame : public SpdyControlFrame {
 public:
  SpdySettingsControlFrame() : SpdyControlFrame(size()) {}
  SpdySettingsControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  uint32 num_entries() const {
    return ntohl(block()->num_entries_);
  }

  void set_num_entries(int val) {
    mutable_block()->num_entries_ = htonl(val);
  }

  int header_block_len() const {
    return length() - (size() - SpdyFrame::kHeaderSize);
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the SpdySettingsControlFrameBlock structure.
  // Note: this is not the size of the SpdySettingsControlFrameBlock class.
  static size_t size() { return sizeof(SpdySettingsControlFrameBlock); }

 private:
  const struct SpdySettingsControlFrameBlock* block() const {
    return static_cast<SpdySettingsControlFrameBlock*>(frame_);
  }
  struct SpdySettingsControlFrameBlock* mutable_block() {
    return static_cast<SpdySettingsControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySettingsControlFrame);
};

class SpdyPingControlFrame : public SpdyControlFrame {
 public:
  SpdyPingControlFrame() : SpdyControlFrame(size()) {}
  SpdyPingControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  uint32 unique_id() const {
    return ntohl(block()->unique_id_);
  }

  void set_unique_id(uint32 unique_id) {
    mutable_block()->unique_id_ = htonl(unique_id);
  }

  static size_t size() { return sizeof(SpdyPingControlFrameBlock); }

 private:
  const struct SpdyPingControlFrameBlock* block() const {
    return static_cast<SpdyPingControlFrameBlock*>(frame_);
  }
  struct SpdyPingControlFrameBlock* mutable_block() {
    return static_cast<SpdyPingControlFrameBlock*>(frame_);
  }
};

class SpdyCredentialControlFrame : public SpdyControlFrame {
 public:
  SpdyCredentialControlFrame() : SpdyControlFrame(size()) {}
  SpdyCredentialControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  const char* payload() const {
    return reinterpret_cast<const char*>(block()) + SpdyFrame::kHeaderSize;
  }

  static size_t size() { return sizeof(SpdyCredentialControlFrameBlock); }

 private:
  const struct SpdyCredentialControlFrameBlock* block() const {
    return static_cast<SpdyCredentialControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyCredentialControlFrame);
};

class SpdyGoAwayControlFrame : public SpdyControlFrame {
 public:
  SpdyGoAwayControlFrame() : SpdyControlFrame(size()) {}
  SpdyGoAwayControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId last_accepted_stream_id() const {
    return ntohl(block()->last_accepted_stream_id_) & kStreamIdMask;
  }

  SpdyGoAwayStatus status() const {
    if (version() < 3) {
      LOG(DFATAL) << "Attempted to access status of SPDY 2 GOAWAY.";
      return GOAWAY_INVALID;
    } else {
      uint32 status = ntohl(block()->status_);
      if (status >= GOAWAY_NUM_STATUS_CODES) {
        return GOAWAY_INVALID;
      } else {
        return static_cast<SpdyGoAwayStatus>(status);
      }
    }
  }

  void set_last_accepted_stream_id(SpdyStreamId id) {
    mutable_block()->last_accepted_stream_id_ = htonl(id & kStreamIdMask);
  }

  static size_t size() { return sizeof(SpdyGoAwayControlFrameBlock); }

 private:
  const struct SpdyGoAwayControlFrameBlock* block() const {
    return static_cast<SpdyGoAwayControlFrameBlock*>(frame_);
  }
  struct SpdyGoAwayControlFrameBlock* mutable_block() {
    return static_cast<SpdyGoAwayControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyGoAwayControlFrame);
};

// A HEADERS frame.
class SpdyHeadersControlFrame : public SpdyControlFrame {
 public:
  SpdyHeadersControlFrame() : SpdyControlFrame(size()) {}
  SpdyHeadersControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  // The number of bytes in the header block beyond the frame header length.
  int header_block_len() const {
    size_t header_block_len = length() - (size() - SpdyFrame::kHeaderSize);
    // SPDY 2 had 2 bytes of unused space preceeding the header block.
    if (version() < 3) {
      header_block_len -= 2;
    }
    return header_block_len;
  }

  const char* header_block() const {
    const char* header_block = reinterpret_cast<const char*>(block()) + size();
    // SPDY 2 had 2 bytes of unused space preceeding the header block.
    if (version() < 3) {
      header_block += 2;
    }
    return header_block;
  }

  // Returns the size of the SpdyHeadersControlFrameBlock structure.
  // Note: this is not the size of the SpdyHeadersControlFrame class.
  static size_t size() { return sizeof(SpdyHeadersControlFrameBlock); }

 private:
  const struct SpdyHeadersControlFrameBlock* block() const {
    return static_cast<SpdyHeadersControlFrameBlock*>(frame_);
  }
  struct SpdyHeadersControlFrameBlock* mutable_block() {
    return static_cast<SpdyHeadersControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyHeadersControlFrame);
};

// A WINDOW_UPDATE frame.
class SpdyWindowUpdateControlFrame : public SpdyControlFrame {
 public:
  SpdyWindowUpdateControlFrame() : SpdyControlFrame(size()) {}
  SpdyWindowUpdateControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  uint32 delta_window_size() const {
    return ntohl(block()->delta_window_size_);
  }

  void set_delta_window_size(uint32 delta_window_size) {
    mutable_block()->delta_window_size_ = htonl(delta_window_size);
  }

  // Returns the size of the SpdyWindowUpdateControlFrameBlock structure.
  // Note: this is not the size of the SpdyWindowUpdateControlFrame class.
  static size_t size() { return sizeof(SpdyWindowUpdateControlFrameBlock); }

 private:
  const struct SpdyWindowUpdateControlFrameBlock* block() const {
    return static_cast<SpdyWindowUpdateControlFrameBlock*>(frame_);
  }
  struct SpdyWindowUpdateControlFrameBlock* mutable_block() {
    return static_cast<SpdyWindowUpdateControlFrameBlock*>(frame_);
  }

  DISALLOW_COPY_AND_ASSIGN(SpdyWindowUpdateControlFrame);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_PROTOCOL_H_
