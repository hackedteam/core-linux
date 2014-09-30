#ifndef _ME_H
#define _ME_H

#ifdef DEBUG
void errorme(char *format, ...);
void debugme(char *format, ...);
#else
#define errorme(x, ...) do{ }while(0)
#define debugme(x, ...) do{ }while(0)
#endif

#include "runtime.h"

#endif /* _ME_H */
