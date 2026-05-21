#include "clock/context.h"

static clock_context_t s_clock_context;

clock_context_t *clock_context_get(void)
{
    return &s_clock_context;
}