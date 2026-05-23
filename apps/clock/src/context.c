#include "clock/context.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static clock_context_t s_clock_context;
static portMUX_TYPE s_clock_context_lock = portMUX_INITIALIZER_UNLOCKED;

clock_context_t *clock_context_get(void)
{
    return &s_clock_context;
}

void clock_context_set_datetime(const clock_rtc_datetime_t *datetime)
{
    if (datetime == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_clock_context_lock);
    s_clock_context.current_datetime = *datetime;
    s_clock_context.current_datetime_valid = true;
    portEXIT_CRITICAL(&s_clock_context_lock);
}

bool clock_context_get_datetime(clock_rtc_datetime_t *out_datetime)
{
    if (out_datetime == NULL) {
        return false;
    }

    portENTER_CRITICAL(&s_clock_context_lock);
    bool valid = s_clock_context.current_datetime_valid;
    if (valid) {
        *out_datetime = s_clock_context.current_datetime;
    }
    portEXIT_CRITICAL(&s_clock_context_lock);

    return valid;
}