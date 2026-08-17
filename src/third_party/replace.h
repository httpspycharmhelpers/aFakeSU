/* aFakeSU: minimal replace.h stub for talloc on bionic.
 * talloc 的 replace.h 由 waf configure 生成; 真机 bionic 无此机制,
 * 此处只提供 talloc.c 编译所需的最小声明。 */
#ifndef __REPLACE_H__
#define __REPLACE_H__

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/param.h>

#define HAVE_STDBOOL_H 1
#define HAVE_CONSTRUCTOR_ATTRIBUTE 1

#define HAVE_INTPTR_T 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_VA_COPY 1
#define HAVE___VA_COPY 1
#define HAVE_SETENV 1
#define HAVE_UNSETENV 1

#ifndef HAVE_MALLOC_STATS
#define HAVE_MALLOC_STATS 1
#endif

#endif /* __REPLACE_H__ */
