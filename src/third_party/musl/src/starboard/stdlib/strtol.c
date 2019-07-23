#include <stdlib.h>
#include "starboard/string.h"

long strtol(const char *restrict s, char **restrict p, int base) {
  return SbStringParseSignedInteger(s, p, base);
}

long long strtoll(const char *restrict s, char **restrict p, int base) {
  return SbStringParseSignedInteger(s, p, base);
}

unsigned long strtoul(const char *restrict s, char **restrict p, int base) {
  return SbStringParseUnsignedInteger(s, p, base);
}

unsigned long long strtoull(const char *restrict s, char **restrict p, int base) {
  return SbStringParseUInt64(s, p, base);
}
