#include <stdlib.h>

float strtof_l(const char *restrict s, char **restrict p,
               struct __locale_struct *l) {
  return strtof(s, p);
}

double strtod_l(const char *restrict s, char **restrict p,
                struct __locale_struct *l) {
  return strtod(s, p);
}

long double strtold_l(const char *restrict s, char **restrict p,
                      struct __locale_struct *l) {
  return strtold(s, p);
}

