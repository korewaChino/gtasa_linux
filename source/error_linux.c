/* error_linux.c -- Linux fatal diagnostics */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

#ifndef GTASA_PRODUCT_NAME
#define GTASA_PRODUCT_NAME "Grand Theft Auto: San Andreas"
#endif

void fatal_error(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "%s: fatal: ", GTASA_PRODUCT_NAME);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}
