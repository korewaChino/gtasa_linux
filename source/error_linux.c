/* error_linux.c -- Linux fatal diagnostics */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void fatal_error(const char *fmt, ...) {
  va_list ap;
  fputs("gtasa_linux: fatal: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}
