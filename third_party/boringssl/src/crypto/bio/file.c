/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#if defined(__linux) || defined(__sun) || defined(__hpux)
// Following definition aliases fopen to fopen64 on above mentioned
// platforms. This makes it possible to open and sequentially access
// files larger than 2GB from 32-bit application. It does not allow to
// traverse them beyond 2GB with fseek/ftell, but on the other hand *no*
// 32-bit platform permits that, not with fseek/ftell. Not to mention
// that breaking 2GB limit for seeking would require surgery to *our*
// API. But sequential access suffices for practical cases when you
// can run into large files, such as fingerprinting, so we can let API
// alone. For reference, the list of 32-bit platforms which allow for
// sequential access of large files without extra "magic" comprise *BSD,
// Darwin, IRIX...
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif

#include <openssl/bio.h>

#if !defined(MS_CALLBACK)
#define MS_CALLBACK
#endif

#if !defined(BIO_FLAGS_UPLINK)
#define BIO_FLAGS_UPLINK 0
#endif

#if !defined(OPENSSL_SYS_STARBOARD)
#include <errno.h>
#include <stdio.h>
#endif  // !defined(OPENSSL_SYS_STARBOARD)
#include <string.h>

#include <openssl/buf.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../internal.h"

#if defined(OPENSSL_SYS_NETWARE) && defined(NETWARE_CLIB)
#include <nwfileio.h>
#endif

#define BIO_FP_READ 0x02
#define BIO_FP_WRITE 0x04
#define BIO_FP_APPEND 0x08

#if !defined(OPENSSL_NO_STDIO)

static int MS_CALLBACK file_write(BIO *h, const char *buf, int num);
static int MS_CALLBACK file_read(BIO *h, char *buf, int size);
static int MS_CALLBACK file_puts(BIO *h, const char *str);
static int MS_CALLBACK file_gets(BIO *h, char *str, int size);
static long MS_CALLBACK file_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int MS_CALLBACK file_new(BIO *h);
static int MS_CALLBACK file_free(BIO *data);
static BIO_METHOD methods_filep = {
    BIO_TYPE_FILE,
    "FILE pointer",
    file_write,
    file_read,
    file_puts,
    file_gets,
    file_ctrl,
    file_new,
    file_free,
    NULL,
};

#if !defined(OPENSSL_SYS_STARBOARD)
static FILE *file_fopen(const char *filename, const char *mode) {
  FILE *file = NULL;

#if defined(_WIN32) && defined(CP_UTF8)
  int sz, len_0 = (int)strlen(filename) + 1;
  DWORD flags;

  /*
   * Basically there are three cases to cover: a) filename is
   * pure ASCII string; b) actual UTF-8 encoded string and
   * c) locale-ized string, i.e. one containing 8-bit
   * characters that are meaningful in current system locale.
   * If filename is pure ASCII or real UTF-8 encoded string,
   * MultiByteToWideChar succeeds and _wfopen works. If
   * filename is locale-ized string, chances are that
   * MultiByteToWideChar fails reporting
   * ERROR_NO_UNICODE_TRANSLATION, in which case we fall
   * back to fopen...
   */
  if ((sz = MultiByteToWideChar(CP_UTF8, (flags = MB_ERR_INVALID_CHARS),
                                filename, len_0, NULL, 0)) > 0 ||
      (GetLastError() == ERROR_INVALID_FLAGS &&
       (sz = MultiByteToWideChar(CP_UTF8, (flags = 0), filename, len_0, NULL,
                                 0)) > 0)) {
    WCHAR wmode[8];
    WCHAR *wfilename = _alloca(sz * sizeof(WCHAR));

    if (MultiByteToWideChar(CP_UTF8, flags, filename, len_0, wfilename, sz) &&
        MultiByteToWideChar(CP_UTF8, 0, mode, strlen(mode) + 1,
                            wmode, sizeof(wmode) / sizeof(wmode[0])) &&
        (file = _wfopen(wfilename, wmode)) == NULL &&
        (OPENSSL_errno == ENOENT || OPENSSL_errno == EBADF)) {
      /*
       * UTF-8 decode succeeded, but no file, filename
       * could still have been locale-ized...
       */
      file = fopen(filename, mode);
    }
  } else if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
    file = fopen(filename, mode);
  }
#else
  file = fopen(filename, mode);
#endif
  return (file);
}

#endif  // defined(OPENSSL_SYS_STARBOARD)

BIO *BIO_new_file(const char *filename, const char *mode) {
  BIO *ret = NULL;
#if defined(OPENSSL_SYS_STARBOARD)
  SbFile sb_file = kSbFileInvalid;
  SbFileError error = kSbFileOk;
  sb_file = SbFileOpen(filename, SbFileModeStringToFlags(mode), NULL, &error);
  if (!SbFileIsValid(sb_file)) {
    OPENSSL_PUT_SYSTEM_ERROR();
    ERR_add_error_data(5, "SbFileOpen('", filename, "','", mode, "')");
    if (error == kSbFileErrorNotFound) {
      OPENSSL_PUT_ERROR(BIO, BIO_R_NO_SUCH_FILE);
    } else {
      OPENSSL_PUT_ERROR(BIO, BIO_R_SYS_LIB);
    }

    return (NULL);
  }
  ret = BIO_new(BIO_s_file());
  if (ret == NULL) {
    SbFileClose(sb_file);
    return (NULL);
  }

  BIO_clear_flags(ret, BIO_FLAGS_UPLINK);
  BIO_set_fp(ret, sb_file, BIO_CLOSE);
#else   // defined(OPENSSL_SYS_STARBOARD)
  FILE *file = file_fopen(filename, mode);

  if (file == NULL) {
#ifdef NATIVE_TARGET_BUILD
    // This block was restored from revision 5470252 of this file.
    OPENSSL_PUT_SYSTEM_ERROR();

    ERR_add_error_data(5, "fopen('", filename, "','", mode, "')");
    if (errno == ENOENT) {
      OPENSSL_PUT_ERROR(BIO, BIO_R_NO_SUCH_FILE);
    } else {
      OPENSSL_PUT_ERROR(BIO, BIO_R_SYS_LIB);
    }
    return NULL;
#else  // NATIVE_TARGET_BUILD
    SYSerr(SYS_F_FOPEN, get_last_sys_error());
    ERR_add_error_data(5, "fopen('", filename, "','", mode, "')");
    if (OPENSSL_errno == ENOENT)
      BIOerr(BIO_F_BIO_NEW_FILE, BIO_R_NO_SUCH_FILE);
    else
      BIOerr(BIO_F_BIO_NEW_FILE, ERR_R_SYS_LIB);
    return (NULL);
#endif  // NATIVE_TARGET_BUILD
  }

  if ((ret = BIO_new(BIO_s_file())) == NULL) {
    fclose(file);
    return (NULL);
  }

  BIO_clear_flags(ret, BIO_FLAGS_UPLINK); /* we did fopen -> we disengage
                                           * UPLINK */

  BIO_set_fp(ret, file, BIO_CLOSE);
#endif  // defined(OPENSSL_SYS_STARBOARD)
  return ret;
}

#if !defined(OPENSSL_NO_FP_API)
BIO *BIO_new_fp(FILE *stream, int close_flag) {
  BIO *ret;

  if ((ret = BIO_new(BIO_s_file())) == NULL)
    return (NULL);

  BIO_set_flags(ret, BIO_FLAGS_UPLINK); /* redundant, left for
                                         * documentation puposes */
  BIO_set_fp(ret, stream, close_flag);
  return ret;
}
#endif  // !defined(OPENSSL_NO_FP_API)

const BIO_METHOD *BIO_s_file(void) { return (&methods_filep); }

static int MS_CALLBACK file_new(BIO *bi) {
  bi->init = 0;
  bi->num = 0;
  bi->ptr = NULL;
  bi->flags = BIO_FLAGS_UPLINK; /* default to UPLINK */
  return (1);
}

static int MS_CALLBACK file_free(BIO *a) {
  if (a == NULL)
    return (0);
  if (a->shutdown) {
    if ((a->init) && (a->ptr != NULL)) {
#if defined(OPENSSL_SYS_STARBOARD)
      SbFileClose((SbFile)a->ptr);
#else   // defined(OPENSSL_SYS_STARBOARD)
// When this file was Starboardized, uplink support was added to the
// non-Starboard code paths. But at this point it's not clear where to find
// definitions for the uplink functions. The latest upstream version of
// BoringSSL, which we will soon upgrade to, does not support uplink. It should
// therefore be ok to just remove uplink support for native target builds while
// we are still on this version of BoringSSL.
#ifndef NATIVE_TARGET_BUILD
      if (a->flags & BIO_FLAGS_UPLINK)
        UP_fclose(a->ptr);
      else
#endif  // !NATIVE_TARGET_BUILD
        fclose(a->ptr);
#endif  // defined(OPENSSL_SYS_STARBOARD)
      a->ptr = NULL;
      a->flags = BIO_FLAGS_UPLINK;
    }
    a->init = 0;
  }

  return (1);
}

static int MS_CALLBACK file_read(BIO *b, char *out, int outl) {
  int ret = 0;
  if (b->init && (out != NULL)) {
#if defined(OPENSSL_SYS_STARBOARD)
    ret = SbFileRead((SbFile)b->ptr, out, outl);
    if (ret < 0) {
      OPENSSL_PUT_SYSTEM_ERROR();
      OPENSSL_PUT_ERROR(BIO, ERR_R_SYS_LIB);
    }
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
    if (b->flags & BIO_FLAGS_UPLINK)
      ret = UP_fread(out, 1, (int)outl, b->ptr);
    else
#endif  // !NATIVE_TARGET_BUILD
      ret = fread(out, 1, (int)outl, (FILE *)b->ptr);
#ifndef NATIVE_TARGET_BUILD
    if (ret == 0 && (b->flags & BIO_FLAGS_UPLINK) ? UP_ferror((FILE *)b->ptr)
                                                  : ferror((FILE *)b->ptr)) {
#else  // !NATIVE_TARGET_BUILD
    if (ret == 0 && ferror((FILE *)b->ptr)) {
#endif  // !NATIVE_TARGET_BUILD
#ifdef NATIVE_TARGET_BUILD
      // TODO(b/270861949): it would be good to verify that error reporting
      // works correctly throughout this file in native builds.
      OPENSSL_PUT_SYSTEM_ERROR();
      OPENSSL_PUT_ERROR(BIO, ERR_R_SYS_LIB);
#else  // NATIVE_TARGET_BUILD
      SYSerr(SYS_F_FREAD, get_last_sys_error());
      BIOerr(BIO_F_FILE_READ, ERR_R_SYS_LIB);
#endif  // NATIVE_TARGET_BUILD
      ret = -1;
    }
#endif  // defined(OPENSSL_SYS_STARBOARD)
  }

  return (ret);
}

static int MS_CALLBACK file_write(BIO *b, const char *in, int inl) {
  int ret = 0;

  if (b->init && (in != NULL)) {
#if defined(OPENSSL_SYS_STARBOARD)
    ret = SbFileWrite((SbFile)b->ptr, in, inl);
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
    if (b->flags & BIO_FLAGS_UPLINK)
      ret = UP_fwrite(in, (int)inl, 1, b->ptr);
    else
#else  // !NATIVE_TARGET_BUILD
      ret = fwrite(in, (int)inl, 1, (FILE *)b->ptr);
#endif  // !NATIVE_TARGET_BUILD
    if (ret)
      ret = inl;
      /* ret=fwrite(in,1,(int)inl,(FILE *)b->ptr); */
      /*
       * according to Tim Hudson <tjh@cryptsoft.com>, the commented out
       * version above can cause 'inl' write calls under some stupid stdio
       * implementations (VMS)
       */
#endif  // defined(OPENSSL_SYS_STARBOARD)
  }
  return ret;
}

static long MS_CALLBACK file_ctrl(BIO *b, int cmd, long num, void *ptr) {
  long ret = 1;
#if defined(OPENSSL_SYS_STARBOARD)
  SbFile fp = (SbFile)b->ptr;
  SbFile *fpp;
#else   // defined(OPENSSL_SYS_STARBOARD)
  FILE *fp = (FILE *)b->ptr;
  FILE **fpp;
#endif  // defined(OPENSSL_SYS_STARBOARD)
  char p[4];

  switch (cmd) {
    case BIO_C_FILE_SEEK:
    case BIO_CTRL_RESET:
#if defined(OPENSSL_SYS_STARBOARD)
      ret = (long)SbFileSeek((SbFile)b->ptr, num, kSbFileFromBegin);
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
      if (b->flags & BIO_FLAGS_UPLINK)
        ret = (long)UP_fseek(b->ptr, num, 0);
      else
#else  // !NATIVE_TARGET_BUILD
        ret = (long)fseek(fp, num, 0);
#endif  // !NATIVE_TARGET_BUILD
#endif  // defined(OPENSSL_SYS_STARBOARD)
      break;
    case BIO_CTRL_EOF:
#if defined(OPENSSL_SYS_STARBOARD)
      ret = (SbFileSeek((SbFile)b->ptr, 0, kSbFileFromCurrent) >=
                     SbFileSeek((SbFile)b->ptr, 0, kSbFileFromEnd)
                 ? 1
                 : 0);
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
      if (b->flags & BIO_FLAGS_UPLINK)
        ret = (long)UP_feof(fp);
      else
#else  // !NATIVE_TARGET_BUILD
        ret = (long)feof(fp);
#endif  // !NATIVE_TARGET_BUILD
#endif  // defined(OPENSSL_SYS_STARBOARD)
      break;
    case BIO_C_FILE_TELL:
    case BIO_CTRL_INFO:
#if defined(OPENSSL_SYS_STARBOARD)
      ret = SbFileSeek((SbFile)b->ptr, 0, kSbFileFromCurrent);
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
      if (b->flags & BIO_FLAGS_UPLINK)
        ret = UP_ftell(b->ptr);
      else
#else  // !NATIVE_TARGET_BUILD
        ret = ftell(fp);
#endif  // !NATIVE_TARGET_BUILD
#endif  // defined(OPENSSL_SYS_STARBOARD)
      break;
    case BIO_C_SET_FILE_PTR:
      file_free(b);
      b->shutdown = (int)num & BIO_CLOSE;
      b->ptr = ptr;
      b->init = 1;
#if BIO_FLAGS_UPLINK != 0
#if defined(__MINGW32__) && defined(__MSVCRT__) && !defined(_IOB_ENTRIES)
#define _IOB_ENTRIES 20
#endif
#if defined(_IOB_ENTRIES)
      /* Safety net to catch purely internal BIO_set_fp calls */
      if ((size_t)ptr >= (size_t)stdin &&
          (size_t)ptr < (size_t)(stdin + _IOB_ENTRIES))
        BIO_clear_flags(b, BIO_FLAGS_UPLINK);
#endif
#endif
#ifdef UP_fsetmod
      if (b->flags & BIO_FLAGS_UPLINK)
        UP_fsetmod(b->ptr, (char)((num & BIO_FP_TEXT) ? 't' : 'b'));
      else
#endif
      {
#if defined(OPENSSL_SYS_WINDOWS)
        int fd = _fileno((FILE *)ptr);
        if (num & BIO_FP_TEXT)
          _setmode(fd, _O_TEXT);
        else
          _setmode(fd, _O_BINARY);
#elif defined(OPENSSL_SYS_NETWARE) && defined(NETWARE_CLIB)
        int fd = fileno((FILE *)ptr);
        /* Under CLib there are differences in file modes */
        if (num & BIO_FP_TEXT)
          setmode(fd, O_TEXT);
        else
          setmode(fd, O_BINARY);
#elif defined(OPENSSL_SYS_MSDOS)
        int fd = fileno((FILE *)ptr);
        /* Set correct text/binary mode */
        if (num & BIO_FP_TEXT)
          _setmode(fd, _O_TEXT);
        /* Dangerous to set stdin/stdout to raw (unless redirected) */
        else {
          if (fd == STDIN_FILENO || fd == STDOUT_FILENO) {
            if (isatty(fd) <= 0)
              _setmode(fd, _O_BINARY);
          } else
            _setmode(fd, _O_BINARY);
        }
#elif defined(OPENSSL_SYS_OS2) || defined(OPENSSL_SYS_WIN32_CYGWIN)
        int fd = fileno((FILE *)ptr);
        if (num & BIO_FP_TEXT)
          setmode(fd, O_TEXT);
        else
          setmode(fd, O_BINARY);
#elif defined(OPENSSL_SYS_STARBOARD)
        /* Do nothing. */
#endif
      }
      break;
    case BIO_C_SET_FILENAME:
      file_free(b);
      b->shutdown = (int)num & BIO_CLOSE;
      if (num & BIO_FP_APPEND) {
        if (num & BIO_FP_READ) {
          BUF_strlcpy(p, "a+", sizeof(p));
        } else {
          BUF_strlcpy(p, "a", sizeof(p));
        }
      } else if ((num & BIO_FP_READ) && (num & BIO_FP_WRITE)) {
        BUF_strlcpy(p, "r+", sizeof(p));
      } else if (num & BIO_FP_WRITE) {
        BUF_strlcpy(p, "w", sizeof(p));
      } else if (num & BIO_FP_READ) {
        BUF_strlcpy(p, "r", sizeof(p));
      } else {
        OPENSSL_PUT_ERROR(BIO, BIO_R_BAD_FOPEN_MODE);
        ret = 0;
        break;
      }
#if defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_WINDOWS) || \
    defined(OPENSSL_SYS_OS2) || defined(OPENSSL_SYS_WIN32_CYGWIN)
      if (!(num & BIO_FP_TEXT))
        strcat(p, "b");
      else
        strcat(p, "t");
#endif
#if defined(OPENSSL_SYS_NETWARE)
      if (!(num & BIO_FP_TEXT))
        strcat(p, "b");
      else
        strcat(p, "t");
#endif
#if defined(OPENSSL_SYS_STARBOARD)
      {
        SbFileError error = kSbFileOk;
        fp = SbFileOpen((const char *)ptr, SbFileModeStringToFlags(p), NULL,
                        &error);
        if (!SbFileIsValid(fp)) {
          OPENSSL_PUT_SYSTEM_ERROR();
          ERR_add_error_data(5, "SbFileOpen('", ptr, "','", p, "')");
          OPENSSL_PUT_ERROR(BIO, ERR_R_SYS_LIB);
          ret = 0;
          break;
        }
        b->ptr = fp;
        b->init = 1;
        BIO_clear_flags(b, BIO_FLAGS_UPLINK); /* disengage UPLINK */
      }
#else   // defined(OPENSSL_SYS_STARBOARD)
      fp = fopen(ptr, p);
      if (fp == NULL) {
#ifdef NATIVE_TARGET_BUILD
        OPENSSL_PUT_SYSTEM_ERROR();
        ERR_add_error_data(5, "fopen('", ptr, "','", p, "')");
        OPENSSL_PUT_ERROR(BIO, ERR_R_SYS_LIB);
#else  // NATIVE_TARGET_BUILD
        SYSerr(SYS_F_FOPEN, get_last_sys_error());
        ERR_add_error_data(5, "fopen('", ptr, "','", p, "')");
        BIOerr(BIO_F_FILE_CTRL, ERR_R_SYS_LIB);
#endif  // NATIVE_TARGET_BUILD
        ret = 0;
        break;
      }
      b->ptr = fp;
      b->init = 1;
      BIO_clear_flags(b, BIO_FLAGS_UPLINK); /* we did fopen -> we disengage
                                             * UPLINK */
#endif  // defined(OPENSSL_SYS_STARBOARD)
      break;
    case BIO_C_GET_FILE_PTR:
#if defined(OPENSSL_SYS_STARBOARD)
      /* the ptr parameter is actually a SbFile * in this case. */
      if (ptr != NULL) {
        fpp = (SbFile *)ptr;
        *fpp = (SbFile)b->ptr;
      }
#else   // defined(OPENSSL_SYS_STARBOARD)
      /* the ptr parameter is actually a FILE ** in this case. */
      if (ptr != NULL) {
        fpp = (FILE **)ptr;
        *fpp = (FILE *)b->ptr;
      }
#endif  // defined(OPENSSL_SYS_STARBOARD)
      break;
    case BIO_CTRL_GET_CLOSE:
      ret = (long)b->shutdown;
      break;
    case BIO_CTRL_SET_CLOSE:
      b->shutdown = (int)num;
      break;
    case BIO_CTRL_FLUSH:
#if defined(OPENSSL_SYS_STARBOARD)
      SbFileFlush((SbFile)b->ptr);
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
      if (b->flags & BIO_FLAGS_UPLINK)
        UP_fflush(b->ptr);
      else
#else  // !NATIVE_TARGET_BUILD
        fflush((FILE *)b->ptr);
#endif  // !NATIVE_TARGET_BUILD
#endif  // defined(OPENSSL_SYS_STARBOARD)
      break;
    case BIO_CTRL_DUP:
      ret = 1;
      break;
    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
      ret = 0;
      break;
  }
  return ret;
}

static int MS_CALLBACK file_gets(BIO *bp, char *buf, int size) {
  int ret = 0;

  buf[0] = '\0';
#if defined(OPENSSL_SYS_STARBOARD)
  ret = -1;
  SbFileInfo info;
  SbFileGetInfo((SbFile)bp->ptr, &info);
  int64_t current = SbFileSeek((SbFile)bp->ptr, kSbFileFromCurrent, 0);
  int64_t remaining = info.size - current;
  int64_t max = (size > remaining ? remaining : size - 1);
  int index = 0;
  for (; index < max; ++index) {
    int count = 0;
    for (;;) {
      count = SbFileRead((SbFile)bp->ptr, buf + index, 1);
      if (count == 0) {
        continue;
      }
      break;
    }
    if (count < 0) {
      return (ret);
    }
    if (buf[index] == '\n') {
      break;
    }
  }
  buf[index] = '\0';
  ret = 0;
#else   // defined(OPENSSL_SYS_STARBOARD)
#ifndef NATIVE_TARGET_BUILD
  if (bp->flags & BIO_FLAGS_UPLINK) {
    if (!UP_fgets(buf, size, bp->ptr))
      goto err;
  } else {
    if (!fgets(buf, size, (FILE *)bp->ptr))
      goto err;
  }
#else  // !NATIVE_TARGET_BUILD
   if (!fgets(buf, size, (FILE *)bp->ptr))
     goto err;
#endif  // !NATIVE_TARGET_BUILD
#endif  // defined(OPENSSL_SYS_STARBOARD)
  if (buf[0] != '\0')
    ret = strlen(buf);
err:
  return ret;
}

static int MS_CALLBACK file_puts(BIO *bp, const char *str) {
  int n, ret;

  n = strlen(str);
  ret = file_write(bp, str, n);
  return (ret);
}

#endif /* OPENSSL_NO_STDIO */

#ifdef NATIVE_TARGET_BUILD
// These definitions were restored from revision 5470252 of this file.
int BIO_get_fp(BIO *bio, FILE **out_file) {
  return BIO_ctrl(bio, BIO_C_GET_FILE_PTR, 0, (char*) out_file);
}

int BIO_set_fp(BIO *bio, FILE *file, int close_flag) {
  return BIO_ctrl(bio, BIO_C_SET_FILE_PTR, close_flag, (char *) file);
}

int BIO_read_filename(BIO *bio, const char *filename) {
  return BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_READ,
                  (char *)filename);
}

int BIO_write_filename(BIO *bio, const char *filename) {
  return BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_WRITE,
                  (char *)filename);
}

int BIO_append_filename(BIO *bio, const char *filename) {
  return BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_APPEND,
                  (char *)filename);
}

int BIO_rw_filename(BIO *bio, const char *filename) {
  return BIO_ctrl(bio, BIO_C_SET_FILENAME,
                  BIO_CLOSE | BIO_FP_READ | BIO_FP_WRITE, (char *)filename);
}
#endif  // NATIVE_TARGET_BUILD
