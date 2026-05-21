#include "ai_talk/store/store.h"

#include "ai_talk/contract.h"

static int s_count = AI_TALK_COUNT_START_VALUE;

void ai_talk_store_set_count(int count)
{
    s_count = count;
}

int ai_talk_store_get_count(void)
{
    return s_count;
}
