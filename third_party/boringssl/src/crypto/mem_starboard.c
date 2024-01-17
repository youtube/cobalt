/* Copyright (C) 2018 */

#include <openssl/opensslconf.h>
#if !defined(OPENSSL_SYS_STARBOARD)
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#else
#include "starboard/memory.h"
#endif  // !defined(OPENSSL_SYS_STARBOARD)

#include <openssl/mem.h>

#include <string.h>

#include <openssl/err.h>

#if defined(OPENSSL_WINDOWS)
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <windows.h>
OPENSSL_MSVC_PRAGMA(warning(pop))
#endif

#include "internal.h"

#define OPENSSL_MALLOC_PREFIX 8

void *OPENSSL_malloc(size_t size) {
  void *ptr = malloc(size + OPENSSL_MALLOC_PREFIX);
  if (ptr == NULL) {
    return NULL;
  }

  *(size_t *)ptr = size;

  return ((uint8_t *)ptr) + OPENSSL_MALLOC_PREFIX;
}

void OPENSSL_free(void *orig_ptr) {
  if (orig_ptr == NULL) {
    return;
  }

  void *ptr = ((uint8_t *)orig_ptr) - OPENSSL_MALLOC_PREFIX;

  size_t size = *(size_t *)ptr;
  OPENSSL_cleanse(ptr, size + OPENSSL_MALLOC_PREFIX);
  free(ptr);
}

void *OPENSSL_realloc(void *orig_ptr, size_t new_size) {
  if (orig_ptr == NULL) {
    return OPENSSL_malloc(new_size);
  }

  void *ptr = ((uint8_t *)orig_ptr) - OPENSSL_MALLOC_PREFIX;
  size_t old_size = *(size_t *)ptr;

  void *ret = OPENSSL_malloc(new_size);
  if (ret == NULL) {
    return NULL;
  }

  size_t to_copy = new_size;
  if (old_size < to_copy) {
    to_copy = old_size;
  }

  memcpy(ret, orig_ptr, to_copy);
  OPENSSL_free(orig_ptr);

  return ret;
}

void OPENSSL_cleanse(void *ptr, size_t len) {
#if defined(OPENSSL_WINDOWS)
  SecureZeroMemory(ptr, len);
#else
  OPENSSL_memset(ptr, 0, len);

#if !defined(OPENSSL_NO_ASM)
  /* As best as we can tell, this is sufficient to break any optimisations that
     might try to eliminate "superfluous" memsets. If there's an easy way to
     detect memset_s, it would be better to use that. */
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
#endif  // !OPENSSL_NO_ASM
}

int CRYPTO_memcmp(const void *in_a, const void *in_b, size_t len) {
  const uint8_t *a = in_a;
  const uint8_t *b = in_b;
  uint8_t x = 0;

  for (size_t i = 0; i < len; i++) {
    x |= a[i] ^ b[i];
  }

  return x;
}

uint32_t OPENSSL_hash32(const void *ptr, size_t len) {
  // These are the FNV-1a parameters for 32 bits.
  static const uint32_t kPrime = 16777619u;
  static const uint32_t kOffsetBasis = 2166136261u;

  const uint8_t *in = ptr;
  uint32_t h = kOffsetBasis;

  for (size_t i = 0; i < len; i++) {
    h ^= in[i];
    h *= kPrime;
  }

  return h;
}

size_t OPENSSL_strnlen(const char *s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (s[i] == 0) {
      return i;
    }
  }

  return len;
}

char *OPENSSL_strdup(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  const size_t len = strlen(s) + 1;
  char *ret = OPENSSL_malloc(len);
  if (ret == NULL) {
    return NULL;
  }
  OPENSSL_memcpy(ret, s, len);
  return ret;
}

int OPENSSL_tolower(int c) {
  if (c >= 'A' && c <= 'Z') {
    return c + ('a' - 'A');
  }
  return c;
}

int OPENSSL_strcasecmp(const char *a, const char *b) {
  for (size_t i = 0;; i++) {
    const int aa = OPENSSL_tolower(a[i]);
    const int bb = OPENSSL_tolower(b[i]);

    if (aa < bb) {
      return -1;
    } else if (aa > bb) {
      return 1;
    } else if (aa == 0) {
      return 0;
    }
  }
}

int OPENSSL_strncasecmp(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    const int aa = OPENSSL_tolower(a[i]);
    const int bb = OPENSSL_tolower(b[i]);

    if (aa < bb) {
      return -1;
    } else if (aa > bb) {
      return 1;
    } else if (aa == 0) {
      return 0;
    }
  }

  return 0;
}

int BIO_snprintf(char *buf, size_t n, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = BIO_vsnprintf(buf, n, format, args);
  va_end(args);
  return ret;
}

int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args) {
#if SB_API_VERSION < 16
  return SbStringFormat(buf, n, format, args);
#else
  return vsnprintf(buf, n, format, args);
#endif
}

char *OPENSSL_strndup(const char *str, size_t size) {
  char *ret;
  size_t alloc_size;

  if (str == NULL) {
    return NULL;
  }

  size = OPENSSL_strnlen(str, size);

  alloc_size = size + 1;
  if (alloc_size < size) {
    // overflow
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  ret = OPENSSL_malloc(alloc_size);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_MALLOC_FAILURE);
    return NULL;
  }

  OPENSSL_memcpy(ret, str, size);
  ret[size] = '\0';
  return ret;
}

size_t OPENSSL_strlcpy(char *dst, const char *src, size_t dst_size) {
  size_t l = 0;

  for (; dst_size > 1 && *src; dst_size--) {
    *dst++ = *src++;
    l++;
  }

  if (dst_size) {
    *dst = 0;
  }

  return l + strlen(src);
}

size_t OPENSSL_strlcat(char *dst, const char *src, size_t dst_size) {
  size_t l = 0;
  for (; dst_size > 0 && *dst; dst_size--, dst++) {
    l++;
  }
  return l + OPENSSL_strlcpy(dst, src, dst_size);
}

void *OPENSSL_memdup(const void *data, size_t size) {
  if (size == 0) {
    return NULL;
  }

  void *ret = OPENSSL_malloc(size);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_MALLOC_FAILURE);
    return NULL;
  }

  OPENSSL_memcpy(ret, data, size);
  return ret;
}
