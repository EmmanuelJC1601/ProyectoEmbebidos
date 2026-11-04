/* shared_state.h - Estado compartido entre hilos.
 * Decision de bitacora (clase 9): banderas y contadores de una palabra como variables atomicas,
 * sin mutex, para que el hilo de pantalla/juego nunca se bloquee esperando a los de entrada. */
#ifndef SHARED_STATE_H
#define SHARED_STATE_H
#include <stdatomic.h>
#include "config.h"

typedef struct {
    atomic_int  btn_level[BUTTON_COUNT];   /* 1 = presionado */
    atomic_uint btn_presses[BUTTON_COUNT]; /* flancos de subida aceptados */
    atomic_uint pot_raw[POT_COUNT];        /* 0..1023 ya filtrado */
    atomic_uint latency_last_us;           /* evento GPIO -> hilo de entrada */
    atomic_uint latency_max_us;
    atomic_uint adc_errors;
    atomic_int  quit;                      /* 1 = todos los hilos terminan */
    atomic_int  poweroff;                  /* 1 = pedir apagado de Linux al salir */
    atomic_int  failed;                    /* 1 = salida por error de hardware */
} shared_t;

static inline void shared_init(shared_t *s)
{
    for (unsigned i = 0; i < BUTTON_COUNT; i++) {
        atomic_init(&s->btn_level[i], 0);
        atomic_init(&s->btn_presses[i], 0u);
    }
    for (unsigned i = 0; i < POT_COUNT; i++)
        atomic_init(&s->pot_raw[i], 0u);
    atomic_init(&s->latency_last_us, 0u);
    atomic_init(&s->latency_max_us, 0u);
    atomic_init(&s->adc_errors, 0u);
    atomic_init(&s->quit, 0);
    atomic_init(&s->poweroff, 0);
    atomic_init(&s->failed, 0);
}
#endif
