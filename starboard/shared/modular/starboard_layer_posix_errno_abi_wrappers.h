// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_ERRNO_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_ERRNO_ABI_WRAPPERS_H_

#include <errno.h>

#include "starboard/export.h"

#define MUSL_EPERM 1
#define MUSL_ENOENT 2
#define MUSL_ESRCH 3
#define MUSL_EINTR 4
#define MUSL_EIO 5
#define MUSL_ENXIO 6
#define MUSL_E2BIG 7
#define MUSL_ENOEXEC 8
#define MUSL_EBADF 9
#define MUSL_ECHILD 10
#define MUSL_EAGAIN 11
#define MUSL_ENOMEM 12
#define MUSL_EACCES 13
#define MUSL_EFAULT 14
#define MUSL_ENOTBLK 15
#define MUSL_EBUSY 16
#define MUSL_EEXIST 17
#define MUSL_EXDEV 18
#define MUSL_ENODEV 19
#define MUSL_ENOTDIR 20
#define MUSL_EISDIR 21
#define MUSL_EINVAL 22
#define MUSL_ENFILE 23
#define MUSL_EMFILE 24
#define MUSL_ENOTTY 25
#define MUSL_ETXTBSY 26
#define MUSL_EFBIG 27
#define MUSL_ENOSPC 28
#define MUSL_ESPIPE 29
#define MUSL_EROFS 30
#define MUSL_EMLINK 31
#define MUSL_EPIPE 32
#define MUSL_EDOM 33
#define MUSL_ERANGE 34
#define MUSL_EDEADLK 35
#define MUSL_ENAMETOOLONG 36
#define MUSL_ENOLCK 37
#define MUSL_ENOSYS 38
#define MUSL_ENOTEMPTY 39
#define MUSL_ELOOP 40
#define MUSL_EWOULDBLOCK MUSL_EAGAIN
#define MUSL_ENOMSG 42
#define MUSL_EIDRM 43
#define MUSL_ECHRNG 44
#define MUSL_EL2NSYNC 45
#define MUSL_EL3HLT 46
#define MUSL_EL3RST 47
#define MUSL_ELNRNG 48
#define MUSL_EUNATCH 49
#define MUSL_ENOCSI 50
#define MUSL_EL2HLT 51
#define MUSL_EBADE 52
#define MUSL_EBADR 53
#define MUSL_EXFULL 54
#define MUSL_ENOANO 55
#define MUSL_EBADRQC 56
#define MUSL_EBADSLT 57
#define MUSL_EDEADLOCK MUSL_EDEADLK
#define MUSL_EBFONT 59
#define MUSL_ENOSTR 60
#define MUSL_ENODATA 61
#define MUSL_ETIME 62
#define MUSL_ENOSR 63
#define MUSL_ENONET 64
#define MUSL_ENOPKG 65
#define MUSL_EREMOTE 66
#define MUSL_ENOLINK 67
#define MUSL_EADV 68
#define MUSL_ESRMNT 69
#define MUSL_ECOMM 70
#define MUSL_EPROTO 71
#define MUSL_EMULTIHOP 72
#define MUSL_EDOTDOT 73
#define MUSL_EBADMSG 74
#define MUSL_EOVERFLOW 75
#define MUSL_ENOTUNIQ 76
#define MUSL_EBADFD 77
#define MUSL_EREMCHG 78
#define MUSL_ELIBACC 79
#define MUSL_ELIBBAD 80
#define MUSL_ELIBSCN 81
#define MUSL_ELIBMAX 82
#define MUSL_ELIBEXEC 83
#define MUSL_EILSEQ 84
#define MUSL_ERESTART 85
#define MUSL_ESTRPIPE 86
#define MUSL_EUSERS 87
#define MUSL_ENOTSOCK 88
#define MUSL_EDESTADDRREQ 89
#define MUSL_EMSGSIZE 90
#define MUSL_EPROTOTYPE 91
#define MUSL_ENOPROTOOPT 92
#define MUSL_EPROTONOSUPPORT 93
#define MUSL_ESOCKTNOSUPPORT 94
#define MUSL_EOPNOTSUPP 95
#define MUSL_ENOTSUP MUSL_EOPNOTSUPP
#define MUSL_EPFNOSUPPORT 96
#define MUSL_EAFNOSUPPORT 97
#define MUSL_EADDRINUSE 98
#define MUSL_EADDRNOTAVAIL 99
#define MUSL_ENETDOWN 100
#define MUSL_ENETUNREACH 101
#define MUSL_ENETRESET 102
#define MUSL_ECONNABORTED 103
#define MUSL_ECONNRESET 104
#define MUSL_ENOBUFS 105
#define MUSL_EISCONN 106
#define MUSL_ENOTCONN 107
#define MUSL_ESHUTDOWN 108
#define MUSL_ETOOMANYREFS 109
#define MUSL_ETIMEDOUT 110
#define MUSL_ECONNREFUSED 111
#define MUSL_EHOSTDOWN 112
#define MUSL_EHOSTUNREACH 113
#define MUSL_EALREADY 114
#define MUSL_EINPROGRESS 115
#define MUSL_ESTALE 116
#define MUSL_EUCLEAN 117
#define MUSL_ENOTNAM 118
#define MUSL_ENAVAIL 119
#define MUSL_EISNAM 120
#define MUSL_EREMOTEIO 121
#define MUSL_EDQUOT 122
#define MUSL_ENOMEDIUM 123
#define MUSL_EMEDIUMTYPE 124
#define MUSL_ECANCELED 125
#define MUSL_ENOKEY 126
#define MUSL_EKEYEXPIRED 127
#define MUSL_EKEYREVOKED 128
#define MUSL_EKEYREJECTED 129
#define MUSL_EOWNERDEAD 130
#define MUSL_ENOTRECOVERABLE 131
#define MUSL_ERFKILL 132
#define MUSL_EHWPOISON 133

inline bool errno_translation() {
#if !defined(EPERM) || (EPERM != MUSL_EPERM) || !defined(ENOENT) ||           \
    (ENOENT != MUSL_ENOENT) || !defined(ESRCH) || (ESRCH != MUSL_ESRCH) ||    \
    !defined(EINTR) || (EINTR != MUSL_EINTR) || !defined(EIO) ||              \
    (EIO != MUSL_EIO) || !defined(ENXIO) || (ENXIO != MUSL_ENXIO) ||          \
    !defined(E2BIG) || (E2BIG != MUSL_E2BIG) || !defined(ENOEXEC) ||          \
    (ENOEXEC != MUSL_ENOEXEC) || !defined(EBADF) || (EBADF != MUSL_EBADF) ||  \
    !defined(ECHILD) || (ECHILD != MUSL_ECHILD) || !defined(EAGAIN) ||        \
    (EAGAIN != MUSL_EAGAIN) || !defined(ENOMEM) || (ENOMEM != MUSL_ENOMEM) || \
    !defined(EACCES) || (EACCES != MUSL_EACCES) || !defined(EFAULT) ||        \
    (EFAULT != MUSL_EFAULT) || !defined(ENOTBLK) ||                           \
    (ENOTBLK != MUSL_ENOTBLK) || !defined(EBUSY) || (EBUSY != MUSL_EBUSY) ||  \
    !defined(EEXIST) || (EEXIST != MUSL_EEXIST) || !defined(EXDEV) ||         \
    (EXDEV != MUSL_EXDEV) || !defined(ENODEV) || (ENODEV != MUSL_ENODEV) ||   \
    !defined(ENOTDIR) || (ENOTDIR != MUSL_ENOTDIR) || !defined(EISDIR) ||     \
    (EISDIR != MUSL_EISDIR) || !defined(EINVAL) || (EINVAL != MUSL_EINVAL) || \
    !defined(ENFILE) || (ENFILE != MUSL_ENFILE) || !defined(EMFILE) ||        \
    (EMFILE != MUSL_EMFILE) || !defined(ENOTTY) || (ENOTTY != MUSL_ENOTTY) || \
    !defined(ETXTBSY) || (ETXTBSY != MUSL_ETXTBSY) || !defined(EFBIG) ||      \
    (EFBIG != MUSL_EFBIG) || !defined(ENOSPC) || (ENOSPC != MUSL_ENOSPC) ||   \
    !defined(ESPIPE) || (ESPIPE != MUSL_ESPIPE) || !defined(EROFS) ||         \
    (EROFS != MUSL_EROFS) || !defined(EMLINK) || (EMLINK != MUSL_EMLINK) ||   \
    !defined(EPIPE) || (EPIPE != MUSL_EPIPE) || !defined(EDOM) ||             \
    (EDOM != MUSL_EDOM) || !defined(ERANGE) || (ERANGE != MUSL_ERANGE) ||     \
    !defined(EDEADLK) || (EDEADLK != MUSL_EDEADLK) ||                         \
    !defined(ENAMETOOLONG) || (ENAMETOOLONG != MUSL_ENAMETOOLONG) ||          \
    !defined(ENOLCK) || (ENOLCK != MUSL_ENOLCK) || !defined(ENOSYS) ||        \
    (ENOSYS != MUSL_ENOSYS) || !defined(ENOTEMPTY) ||                         \
    (ENOTEMPTY != MUSL_ENOTEMPTY) || !defined(ELOOP) ||                       \
    (ELOOP != MUSL_ELOOP) || !defined(EWOULDBLOCK) ||                         \
    (EWOULDBLOCK != MUSL_EWOULDBLOCK) || !defined(ENOMSG) ||                  \
    (ENOMSG != MUSL_ENOMSG) || !defined(EIDRM) || (EIDRM != MUSL_EIDRM) ||    \
    !defined(ECHRNG) || (ECHRNG != MUSL_ECHRNG) || !defined(EL2NSYNC) ||      \
    (EL2NSYNC != MUSL_EL2NSYNC) || !defined(EL3HLT) ||                        \
    (EL3HLT != MUSL_EL3HLT) || !defined(EL3RST) || (EL3RST != MUSL_EL3RST) || \
    !defined(ELNRNG) || (ELNRNG != MUSL_ELNRNG) || !defined(EUNATCH) ||       \
    (EUNATCH != MUSL_EUNATCH) || !defined(ENOCSI) ||                          \
    (ENOCSI != MUSL_ENOCSI) || !defined(EL2HLT) || (EL2HLT != MUSL_EL2HLT) || \
    !defined(EBADE) || (EBADE != MUSL_EBADE) || !defined(EBADR) ||            \
    (EBADR != MUSL_EBADR) || !defined(EXFULL) || (EXFULL != MUSL_EXFULL) ||   \
    !defined(ENOANO) || (ENOANO != MUSL_ENOANO) || !defined(EBADRQC) ||       \
    (EBADRQC != MUSL_EBADRQC) || !defined(EBADSLT) ||                         \
    (EBADSLT != MUSL_EBADSLT) || !defined(EDEADLK) ||                         \
    (EDEADLK != MUSL_EDEADLK) || !defined(EBFONT) ||                          \
    (EBFONT != MUSL_EBFONT) || !defined(ENOSTR) || (ENOSTR != MUSL_ENOSTR) || \
    !defined(ENODATA) || (ENODATA != MUSL_ENODATA) || !defined(ETIME) ||      \
    (ETIME != MUSL_ETIME) || !defined(ENOSR) || (ENOSR != MUSL_ENOSR) ||      \
    !defined(ENONET) || (ENONET != MUSL_ENONET) || !defined(ENOPKG) ||        \
    (ENOPKG != MUSL_ENOPKG) || !defined(EREMOTE) ||                           \
    (EREMOTE != MUSL_EREMOTE) || !defined(ENOLINK) ||                         \
    (ENOLINK != MUSL_ENOLINK) || !defined(EADV) || (EADV != MUSL_EADV) ||     \
    !defined(ESRMNT) || (ESRMNT != MUSL_ESRMNT) || !defined(ECOMM) ||         \
    (ECOMM != MUSL_ECOMM) || !defined(EPROTO) || (EPROTO != MUSL_EPROTO) ||   \
    !defined(EMULTIHOP) || (EMULTIHOP != MUSL_EMULTIHOP) ||                   \
    !defined(EDOTDOT) || (EDOTDOT != MUSL_EDOTDOT) || !defined(EBADMSG) ||    \
    (EBADMSG != MUSL_EBADMSG) || !defined(EOVERFLOW) ||                       \
    (EOVERFLOW != MUSL_EOVERFLOW) || !defined(ENOTUNIQ) ||                    \
    (ENOTUNIQ != MUSL_ENOTUNIQ) || !defined(EBADFD) ||                        \
    (EBADFD != MUSL_EBADFD) || !defined(EREMCHG) ||                           \
    (EREMCHG != MUSL_EREMCHG) || !defined(ELIBACC) ||                         \
    (ELIBACC != MUSL_ELIBACC) || !defined(ELIBBAD) ||                         \
    (ELIBBAD != MUSL_ELIBBAD) || !defined(ELIBSCN) ||                         \
    (ELIBSCN != MUSL_ELIBSCN) || !defined(ELIBMAX) ||                         \
    (ELIBMAX != MUSL_ELIBMAX) || !defined(ELIBEXEC) ||                        \
    (ELIBEXEC != MUSL_ELIBEXEC) || !defined(EILSEQ) ||                        \
    (EILSEQ != MUSL_EILSEQ) || !defined(ERESTART) ||                          \
    (ERESTART != MUSL_ERESTART) || !defined(ESTRPIPE) ||                      \
    (ESTRPIPE != MUSL_ESTRPIPE) || !defined(EUSERS) ||                        \
    (EUSERS != MUSL_EUSERS) || !defined(ENOTSOCK) ||                          \
    (ENOTSOCK != MUSL_ENOTSOCK) || !defined(EDESTADDRREQ) ||                  \
    (EDESTADDRREQ != MUSL_EDESTADDRREQ) || !defined(EMSGSIZE) ||              \
    (EMSGSIZE != MUSL_EMSGSIZE) || !defined(EPROTOTYPE) ||                    \
    (EPROTOTYPE != MUSL_EPROTOTYPE) || !defined(ENOPROTOOPT) ||               \
    (ENOPROTOOPT != MUSL_ENOPROTOOPT) || !defined(EPROTONOSUPPORT) ||         \
    (EPROTONOSUPPORT != MUSL_EPROTONOSUPPORT) || !defined(ESOCKTNOSUPPORT) || \
    (ESOCKTNOSUPPORT != MUSL_ESOCKTNOSUPPORT) || !defined(EOPNOTSUPP) ||      \
    (EOPNOTSUPP != MUSL_EOPNOTSUPP) || !defined(EOPNOTSUPP) ||                \
    (EOPNOTSUPP != MUSL_EOPNOTSUPP) || !defined(EPFNOSUPPORT) ||              \
    (EPFNOSUPPORT != MUSL_EPFNOSUPPORT) || !defined(EAFNOSUPPORT) ||          \
    (EAFNOSUPPORT != MUSL_EAFNOSUPPORT) || !defined(EADDRINUSE) ||            \
    (EADDRINUSE != MUSL_EADDRINUSE) || !defined(EADDRNOTAVAIL) ||             \
    (EADDRNOTAVAIL != MUSL_EADDRNOTAVAIL) || !defined(ENETDOWN) ||            \
    (ENETDOWN != MUSL_ENETDOWN) || !defined(ENETUNREACH) ||                   \
    (ENETUNREACH != MUSL_ENETUNREACH) || !defined(ENETRESET) ||               \
    (ENETRESET != MUSL_ENETRESET) || !defined(ECONNABORTED) ||                \
    (ECONNABORTED != MUSL_ECONNABORTED) || !defined(ECONNRESET) ||            \
    (ECONNRESET != MUSL_ECONNRESET) || !defined(ENOBUFS) ||                   \
    (ENOBUFS != MUSL_ENOBUFS) || !defined(EISCONN) ||                         \
    (EISCONN != MUSL_EISCONN) || !defined(ENOTCONN) ||                        \
    (ENOTCONN != MUSL_ENOTCONN) || !defined(ESHUTDOWN) ||                     \
    (ESHUTDOWN != MUSL_ESHUTDOWN) || !defined(ETOOMANYREFS) ||                \
    (ETOOMANYREFS != MUSL_ETOOMANYREFS) || !defined(ETIMEDOUT) ||             \
    (ETIMEDOUT != MUSL_ETIMEDOUT) || !defined(ECONNREFUSED) ||                \
    (ECONNREFUSED != MUSL_ECONNREFUSED) || !defined(EHOSTDOWN) ||             \
    (EHOSTDOWN != MUSL_EHOSTDOWN) || !defined(EHOSTUNREACH) ||                \
    (EHOSTUNREACH != MUSL_EHOSTUNREACH) || !defined(EALREADY) ||              \
    (EALREADY != MUSL_EALREADY) || !defined(EINPROGRESS) ||                   \
    (EINPROGRESS != MUSL_EINPROGRESS) || !defined(ESTALE) ||                  \
    (ESTALE != MUSL_ESTALE) || !defined(EUCLEAN) ||                           \
    (EUCLEAN != MUSL_EUCLEAN) || !defined(ENOTNAM) ||                         \
    (ENOTNAM != MUSL_ENOTNAM) || !defined(ENAVAIL) ||                         \
    (ENAVAIL != MUSL_ENAVAIL) || !defined(EISNAM) ||                          \
    (EISNAM != MUSL_EISNAM) || !defined(EREMOTEIO) ||                         \
    (EREMOTEIO != MUSL_EREMOTEIO) || !defined(EDQUOT) ||                      \
    (EDQUOT != MUSL_EDQUOT) || !defined(ENOMEDIUM) ||                         \
    (ENOMEDIUM != MUSL_ENOMEDIUM) || !defined(EMEDIUMTYPE) ||                 \
    (EMEDIUMTYPE != MUSL_EMEDIUMTYPE) || !defined(ECANCELED) ||               \
    (ECANCELED != MUSL_ECANCELED) || !defined(ENOKEY) ||                      \
    (ENOKEY != MUSL_ENOKEY) || !defined(EKEYEXPIRED) ||                       \
    (EKEYEXPIRED != MUSL_EKEYEXPIRED) || !defined(EKEYREVOKED) ||             \
    (EKEYREVOKED != MUSL_EKEYREVOKED) || !defined(EKEYREJECTED) ||            \
    (EKEYREJECTED != MUSL_EKEYREJECTED) || !defined(EOWNERDEAD) ||            \
    (EOWNERDEAD != MUSL_EOWNERDEAD) || !defined(ENOTRECOVERABLE) ||           \
    (ENOTRECOVERABLE != MUSL_ENOTRECOVERABLE) || !defined(ERFKILL) ||         \
    (ERFKILL != MUSL_ERFKILL) || !defined(EHWPOISON) ||                       \
    (EHWPOISON != MUSL_EHWPOISON)
  return true;
#else
  return false;
#endif
}

inline int errno_to_musl_errno(int platform_errno) {
  switch (platform_errno) {
    case 0:
      return 0;
#ifdef EPERM
    case EPERM:
      return MUSL_EPERM;
#endif
#ifdef ENOENT
    case ENOENT:
      return MUSL_ENOENT;
#endif
#ifdef ESRCH
    case ESRCH:
      return MUSL_ESRCH;
#endif
#ifdef EINTR
    case EINTR:
      return MUSL_EINTR;
#endif
#ifdef EIO
    case EIO:
      return MUSL_EIO;
#endif
#ifdef ENXIO
    case ENXIO:
      return MUSL_ENXIO;
#endif
#ifdef E2BIG
    case E2BIG:
      return MUSL_E2BIG;
#endif
#ifdef ENOEXEC
    case ENOEXEC:
      return MUSL_ENOEXEC;
#endif
#ifdef EBADF
    case EBADF:
      return MUSL_EBADF;
#endif
#ifdef ECHILD
    case ECHILD:
      return MUSL_ECHILD;
#endif
#ifdef EAGAIN
    case EAGAIN:
      return MUSL_EAGAIN;
#endif
#ifdef ENOMEM
    case ENOMEM:
      return MUSL_ENOMEM;
#endif
#ifdef EACCES
    case EACCES:
      return MUSL_EACCES;
#endif
#ifdef EFAULT
    case EFAULT:
      return MUSL_EFAULT;
#endif
#ifdef ENOTBLK
    case ENOTBLK:
      return MUSL_ENOTBLK;
#endif
#ifdef EBUSY
    case EBUSY:
      return MUSL_EBUSY;
#endif
#ifdef EEXIST
    case EEXIST:
      return MUSL_EEXIST;
#endif
#ifdef EXDEV
    case EXDEV:
      return MUSL_EXDEV;
#endif
#ifdef ENODEV
    case ENODEV:
      return MUSL_ENODEV;
#endif
#ifdef ENOTDIR
    case ENOTDIR:
      return MUSL_ENOTDIR;
#endif
#ifdef EISDIR
    case EISDIR:
      return MUSL_EISDIR;
#endif
#ifdef EINVAL
    case EINVAL:
      return MUSL_EINVAL;
#endif
#ifdef ENFILE
    case ENFILE:
      return MUSL_ENFILE;
#endif
#ifdef EMFILE
    case EMFILE:
      return MUSL_EMFILE;
#endif
#ifdef ENOTTY
    case ENOTTY:
      return MUSL_ENOTTY;
#endif
#ifdef ETXTBSY
    case ETXTBSY:
      return MUSL_ETXTBSY;
#endif
#ifdef EFBIG
    case EFBIG:
      return MUSL_EFBIG;
#endif
#ifdef ENOSPC
    case ENOSPC:
      return MUSL_ENOSPC;
#endif
#ifdef ESPIPE
    case ESPIPE:
      return MUSL_ESPIPE;
#endif
#ifdef EROFS
    case EROFS:
      return MUSL_EROFS;
#endif
#ifdef EMLINK
    case EMLINK:
      return MUSL_EMLINK;
#endif
#ifdef EPIPE
    case EPIPE:
      return MUSL_EPIPE;
#endif
#ifdef EDOM
    case EDOM:
      return MUSL_EDOM;
#endif
#ifdef ERANGE
    case ERANGE:
      return MUSL_ERANGE;
#endif
#ifdef EDEADLK
    case EDEADLK:
      return MUSL_EDEADLK;
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
      return MUSL_ENAMETOOLONG;
#endif
#ifdef ENOLCK
    case ENOLCK:
      return MUSL_ENOLCK;
#endif
#ifdef ENOSYS
    case ENOSYS:
      return MUSL_ENOSYS;
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY:
      return MUSL_ENOTEMPTY;
#endif
#ifdef ELOOP
    case ELOOP:
      return MUSL_ELOOP;
#endif
#ifdef ENOMSG
    case ENOMSG:
      return MUSL_ENOMSG;
#endif
#ifdef EIDRM
    case EIDRM:
      return MUSL_EIDRM;
#endif
#ifdef ECHRNG
    case ECHRNG:
      return MUSL_ECHRNG;
#endif
#ifdef EL2NSYNC
    case EL2NSYNC:
      return MUSL_EL2NSYNC;
#endif
#ifdef EL3HLT
    case EL3HLT:
      return MUSL_EL3HLT;
#endif
#ifdef EL3RST
    case EL3RST:
      return MUSL_EL3RST;
#endif
#ifdef ELNRNG
    case ELNRNG:
      return MUSL_ELNRNG;
#endif
#ifdef EUNATCH
    case EUNATCH:
      return MUSL_EUNATCH;
#endif
#ifdef ENOCSI
    case ENOCSI:
      return MUSL_ENOCSI;
#endif
#ifdef EL2HLT
    case EL2HLT:
      return MUSL_EL2HLT;
#endif
#ifdef EBADE
    case EBADE:
      return MUSL_EBADE;
#endif
#ifdef EBADR
    case EBADR:
      return MUSL_EBADR;
#endif
#ifdef EXFULL
    case EXFULL:
      return MUSL_EXFULL;
#endif
#ifdef ENOANO
    case ENOANO:
      return MUSL_ENOANO;
#endif
#ifdef EBADRQC
    case EBADRQC:
      return MUSL_EBADRQC;
#endif
#ifdef EBADSLT
    case EBADSLT:
      return MUSL_EBADSLT;
#endif
#ifdef EBFONT
    case EBFONT:
      return MUSL_EBFONT;
#endif
#ifdef ENOSTR
    case ENOSTR:
      return MUSL_ENOSTR;
#endif
#ifdef ENODATA
    case ENODATA:
      return MUSL_ENODATA;
#endif
#ifdef ETIME
    case ETIME:
      return MUSL_ETIME;
#endif
#ifdef ENOSR
    case ENOSR:
      return MUSL_ENOSR;
#endif
#ifdef ENONET
    case ENONET:
      return MUSL_ENONET;
#endif
#ifdef ENOPKG
    case ENOPKG:
      return MUSL_ENOPKG;
#endif
#ifdef EREMOTE
    case EREMOTE:
      return MUSL_EREMOTE;
#endif
#ifdef ENOLINK
    case ENOLINK:
      return MUSL_ENOLINK;
#endif
#ifdef EADV
    case EADV:
      return MUSL_EADV;
#endif
#ifdef ESRMNT
    case ESRMNT:
      return MUSL_ESRMNT;
#endif
#ifdef ECOMM
    case ECOMM:
      return MUSL_ECOMM;
#endif
#ifdef EPROTO
    case EPROTO:
      return MUSL_EPROTO;
#endif
#ifdef EMULTIHOP
    case EMULTIHOP:
      return MUSL_EMULTIHOP;
#endif
#ifdef EDOTDOT
    case EDOTDOT:
      return MUSL_EDOTDOT;
#endif
#ifdef EBADMSG
    case EBADMSG:
      return MUSL_EBADMSG;
#endif
#ifdef EOVERFLOW
    case EOVERFLOW:
      return MUSL_EOVERFLOW;
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ:
      return MUSL_ENOTUNIQ;
#endif
#ifdef EBADFD
    case EBADFD:
      return MUSL_EBADFD;
#endif
#ifdef EREMCHG
    case EREMCHG:
      return MUSL_EREMCHG;
#endif
#ifdef ELIBACC
    case ELIBACC:
      return MUSL_ELIBACC;
#endif
#ifdef ELIBBAD
    case ELIBBAD:
      return MUSL_ELIBBAD;
#endif
#ifdef ELIBSCN
    case ELIBSCN:
      return MUSL_ELIBSCN;
#endif
#ifdef ELIBMAX
    case ELIBMAX:
      return MUSL_ELIBMAX;
#endif
#ifdef ELIBEXEC
    case ELIBEXEC:
      return MUSL_ELIBEXEC;
#endif
#ifdef EILSEQ
    case EILSEQ:
      return MUSL_EILSEQ;
#endif
#ifdef ERESTART
    case ERESTART:
      return MUSL_ERESTART;
#endif
#ifdef ESTRPIPE
    case ESTRPIPE:
      return MUSL_ESTRPIPE;
#endif
#ifdef EUSERS
    case EUSERS:
      return MUSL_EUSERS;
#endif
#ifdef ENOTSOCK
    case ENOTSOCK:
      return MUSL_ENOTSOCK;
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ:
      return MUSL_EDESTADDRREQ;
#endif
#ifdef EMSGSIZE
    case EMSGSIZE:
      return MUSL_EMSGSIZE;
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE:
      return MUSL_EPROTOTYPE;
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT:
      return MUSL_ENOPROTOOPT;
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT:
      return MUSL_EPROTONOSUPPORT;
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT:
      return MUSL_ESOCKTNOSUPPORT;
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP:
      return MUSL_EOPNOTSUPP;
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT:
      return MUSL_EPFNOSUPPORT;
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT:
      return MUSL_EAFNOSUPPORT;
#endif
#ifdef EADDRINUSE
    case EADDRINUSE:
      return MUSL_EADDRINUSE;
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL:
      return MUSL_EADDRNOTAVAIL;
#endif
#ifdef ENETDOWN
    case ENETDOWN:
      return MUSL_ENETDOWN;
#endif
#ifdef ENETUNREACH
    case ENETUNREACH:
      return MUSL_ENETUNREACH;
#endif
#ifdef ENETRESET
    case ENETRESET:
      return MUSL_ENETRESET;
#endif
#ifdef ECONNABORTED
    case ECONNABORTED:
      return MUSL_ECONNABORTED;
#endif
#ifdef ECONNRESET
    case ECONNRESET:
      return MUSL_ECONNRESET;
#endif
#ifdef ENOBUFS
    case ENOBUFS:
      return MUSL_ENOBUFS;
#endif
#ifdef EISCONN
    case EISCONN:
      return MUSL_EISCONN;
#endif
#ifdef ENOTCONN
    case ENOTCONN:
      return MUSL_ENOTCONN;
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN:
      return MUSL_ESHUTDOWN;
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS:
      return MUSL_ETOOMANYREFS;
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT:
      return MUSL_ETIMEDOUT;
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED:
      return MUSL_ECONNREFUSED;
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN:
      return MUSL_EHOSTDOWN;
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH:
      return MUSL_EHOSTUNREACH;
#endif
#ifdef EALREADY
    case EALREADY:
      return MUSL_EALREADY;
#endif
#ifdef EINPROGRESS
    case EINPROGRESS:
      return MUSL_EINPROGRESS;
#endif
#ifdef ESTALE
    case ESTALE:
      return MUSL_ESTALE;
#endif
#ifdef EUCLEAN
    case EUCLEAN:
      return MUSL_EUCLEAN;
#endif
#ifdef ENOTNAM
    case ENOTNAM:
      return MUSL_ENOTNAM;
#endif
#ifdef ENAVAIL
    case ENAVAIL:
      return MUSL_ENAVAIL;
#endif
#ifdef EISNAM
    case EISNAM:
      return MUSL_EISNAM;
#endif
#ifdef EREMOTEIO
    case EREMOTEIO:
      return MUSL_EREMOTEIO;
#endif
#ifdef EDQUOT
    case EDQUOT:
      return MUSL_EDQUOT;
#endif
#ifdef ENOMEDIUM
    case ENOMEDIUM:
      return MUSL_ENOMEDIUM;
#endif
#ifdef EMEDIUMTYPE
    case EMEDIUMTYPE:
      return MUSL_EMEDIUMTYPE;
#endif
#ifdef ECANCELED
    case ECANCELED:
      return MUSL_ECANCELED;
#endif
#ifdef ENOKEY
    case ENOKEY:
      return MUSL_ENOKEY;
#endif
#ifdef EKEYEXPIRED
    case EKEYEXPIRED:
      return MUSL_EKEYEXPIRED;
#endif
#ifdef EKEYREVOKED
    case EKEYREVOKED:
      return MUSL_EKEYREVOKED;
#endif
#ifdef EKEYREJECTED
    case EKEYREJECTED:
      return MUSL_EKEYREJECTED;
#endif
#ifdef EOWNERDEAD
    case EOWNERDEAD:
      return MUSL_EOWNERDEAD;
#endif
#ifdef ENOTRECOVERABLE
    case ENOTRECOVERABLE:
      return MUSL_ENOTRECOVERABLE;
#endif
#ifdef ERFKILL
    case ERFKILL:
      return MUSL_ERFKILL;
#endif
#ifdef EHWPOISON
    case EHWPOISON:
      return MUSL_EHWPOISON;
#endif
    default:
      return MUSL_EINVAL;
  }
}

#ifdef __cplusplus
extern "C" {
#endif

SB_EXPORT int* __abi_wrap___errno_location();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_ERRNO_ABI_WRAPPERS_H_
