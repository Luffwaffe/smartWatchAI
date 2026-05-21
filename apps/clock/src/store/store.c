#include "clock/store/store.h"

#include "clock/contract.h"

static int s_count = CLOCK_COUNTDOWN_START_VALUE;

void clock_store_set_count(int count)
{
    s_count = count;
}

int clock_store_get_count(void)
{
    return s_count;
}