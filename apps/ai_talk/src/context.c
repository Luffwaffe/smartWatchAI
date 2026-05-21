#include "ai_talk/context.h"

static ai_talk_context_t s_ai_talk_context;

ai_talk_context_t *ai_talk_context_get(void)
{
    return &s_ai_talk_context;
}
