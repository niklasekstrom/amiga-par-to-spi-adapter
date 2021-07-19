#ifndef MISC_PROTOS_H
#define MISC_PROTOS_H

#include <exec/types.h>

UBYTE *AllocMiscResource(__reg("a6") struct Library *resource, __reg("d0") ULONG unitNum, __reg("a1") const char *name)="\tjsr\t-6(a6)";

void FreeMiscResource(__reg("a6") struct Library *resource, __reg("d0") ULONG unitNum)="\tjsr\t-12(a6)";

#endif   /* MISC_PROTOS_H */
