#include "filter.h"
#include "config.h"

uint16_t pot_filter_step(pot_filter_t *f, uint16_t sample)
{
    if (sample > ADC_MAX)
        sample = ADC_MAX;
    if (!f->primed) {
        f->value = sample;
        f->primed = true;
        return f->value;
    }
    int diff = (int)sample - (int)f->value;
    int mag = diff < 0 ? -diff : diff;
    if ((unsigned)mag <= ADC_DEADBAND)
        return f->value;                       /* ruido: no mover */

    int step = diff / (1 << ADC_FILTER_SHIFT); /* division: evita >> sobre negativos */
    if (step == 0)
        step = diff > 0 ? 1 : -1;              /* garantiza converger */
    f->value = (uint16_t)((int)f->value + step);
    return f->value;
}

uint32_t adc_scale(uint16_t raw, uint32_t out_max)
{
    if (raw > ADC_MAX)
        raw = ADC_MAX;
    return ((uint32_t)raw * out_max) / ADC_MAX;
}

bool hold_update(hold_t *h, bool condition, int64_t now_ms, int64_t threshold_ms)
{
    if (!condition) {
        h->active = false;
        h->fired = false;
        return false;
    }
    if (!h->active) {
        h->active = true;
        h->since_ms = now_ms;
        return false;
    }
    if (!h->fired && (now_ms - h->since_ms) >= threshold_ms) {
        h->fired = true;
        return true;
    }
    return false;
}
