#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
char g_ident[128];
}

void openlog(const char* ident, int option, int facility) {
  if (ident) {
    strncpy(g_ident, ident, sizeof(g_ident) - 1);
    g_ident[sizeof(g_ident) - 1] = '\0';
  } else {
    g_ident[0] = '\0';
  }
}

void syslog(int priority, const char* format, ...) {
  char syslog_message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(syslog_message, sizeof(syslog_message), format, args);
  va_end(args);

  if (g_ident[0] != '\0') {
    printf("%s: %s\n", g_ident, syslog_message);
  } else {
    printf("%s\n", syslog_message);
  }
}

void closelog(void) {
  g_ident[0] = '\0';
}
