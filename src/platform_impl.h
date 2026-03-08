#ifndef PROLOG_FREESTANDING
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "prolog.h"

#ifndef PROLOG_FREESTANDING
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static inline long long prolog_file_mtime(const char *path) {
  struct stat st;
  return (stat(path, &st) == 0) ? (long long)st.st_mtime : -1LL;
}

#else // PROLOG_FREESTANDING

// user may define prolog_file_mtime(path) -> long long before including this
// header; if not defined, make/0 treats every file as always-changed
#ifndef prolog_file_mtime
#define prolog_file_mtime(path) (-1LL)
#endif

#endif