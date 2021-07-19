#ifndef CIA_PROTOS_H
#define CIA_PROTOS_H

#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>

struct Interrupt *AddICRVector(__reg("a6") struct Library *resource, __reg("d0") LONG iCRBit, __reg("a1") struct Interrupt *interrupt)="\tjsr\t-6(a6)";

void RemICRVector(__reg("a6") struct Library *resource, __reg("d0") LONG iCRBit, __reg("a1") struct Interrupt *interrupt)="\tjsr\t-12(a6)";

WORD AbleICR(__reg("a6") struct Library *resource, __reg("d0") LONG mask)="\tjsr\t-18(a6)";

WORD SetICR(__reg("a6") struct Library *resource, __reg("d0") LONG mask)="\tjsr\t-24(a6)";

#endif /* CIA_PROTOS_H */
