#include <stdlib.h>
#include "starboard/string.h"

// Starboard does not provide string to floating point number conversion at
// higher or lower precision than a double. This works for floats since a double
// is guaranteed to be at least as precise as a float, however, we would lose
// precision on systems where a long double is more precise than a double.
// However, since a long double is only required to be as precise as a double,
// the loss of precision is acceptable.

float strtof(const char *restrict s, char **restrict p) {
  return SbStringParseDouble(s, p);
}

double strtod(const char *restrict s, char **restrict p) {
  return SbStringParseDouble(s, p);
}

long double strtold(const char *restrict s, char **restrict p) {
  return SbStringParseDouble(s, p);
}

float strtof_l(const char *restrict s, char **restrict p,
               struct __locale_struct *l) {
  return SbStringParseDouble(s, p);
}

double strtod_l(const char *restrict s, char **restrict p,
                struct __locale_struct *l) {
  return SbStringParseDouble(s, p);
}

long double strtold_l(const char *restrict s, char **restrict p,
                      struct __locale_struct *l) {
  return SbStringParseDouble(s, p);
}

