#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define err_abort(code, text)                                                               \
    do {                                                                                    \
        fprintf(stderr, "%s at \"%s\":%d: %s\n", text, __FILE__, __LINE__, strerror(code)); \
        abort();                                                                            \
    } while (0);

#define errno_abort(text)                                                                    \
    do {                                                                                     \
        fprintf(stderr, "%s at \"%s\":%d: %s\n", text, __FILE__, __LINE__, strerror(errno)); \
        abort();                                                                             \
    } while (0);

#endif