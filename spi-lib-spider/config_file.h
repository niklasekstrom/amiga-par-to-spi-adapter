#include <exec/types.h>

struct ClockportConfig
{
    ULONG clockport_address;
    ULONG interrupt_number;
};

extern void read_and_parse_config_file(struct ClockportConfig *cfg);
