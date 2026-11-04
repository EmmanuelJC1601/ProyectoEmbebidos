/* filter.h - Logica pura (sin hardware) para poder probarla en cualquier PC. */
#ifndef FILTER_H
#define FILTER_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t value;
    bool     primed;
} pot_filter_t;

/* Filtro con zona muerta: ignora ruido de +-ADC_DEADBAND y suaviza el resto. */
uint16_t pot_filter_step(pot_filter_t *f, uint16_t sample);

/* Escala 0..ADC_MAX a 0..out_max (p. ej. fila de la paleta de Pong o porcentaje). */
uint32_t adc_scale(uint16_t raw, uint32_t out_max);

/* Detecta una condicion mantenida (ambos botones) durante threshold_ms. Devuelve true una vez. */
typedef struct {
    int64_t since_ms;
    bool    active;
    bool    fired;
} hold_t;
bool hold_update(hold_t *h, bool condition, int64_t now_ms, int64_t threshold_ms);
#endif
