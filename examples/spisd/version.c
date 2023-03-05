#include "version.h"

#define STR(x) #x
#define XSTR(x) STR(x)

char device_name[] = NAME ".device";
char id_string[] = NAME " " XSTR(VERSION) "." XSTR(REVISION) " (" DATE ")\n\r";

