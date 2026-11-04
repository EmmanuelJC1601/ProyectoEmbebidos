/* buttons.h - Botones por libgpiod con eventos de flanco (sin polling).
 * API 2 (Trixie): GPIOD_API_V2, debounce en el kernel.
 * API 1.6 (Bookworm): debounce de BUTTON_DEBOUNCE_US en este modulo. */
#ifndef BUTTONS_H
#define BUTTONS_H
#include "config.h"
#include "shared_state.h"
#include <stdint.h>

#if defined(GPIOD_API_V2)
struct gpiod_chip;
struct gpiod_line_request;
struct gpiod_edge_event_buffer;

typedef struct {
    struct gpiod_chip              *chip;
    struct gpiod_line_request      *req;
    struct gpiod_edge_event_buffer *buf;
} buttons_t;
#else
struct gpiod_chip;
struct gpiod_line;

typedef struct {
    struct gpiod_chip *chip;
    struct gpiod_line *line[BUTTON_COUNT];
    int                requested;
    int                pending[BUTTON_COUNT];     /* 1 = hay un cambio aun no confirmado */
    int64_t            pending_ns[BUTTON_COUNT];  /* timestamp del ultimo flanco (CLOCK_MONOTONIC) */
} buttons_t;
#endif

int  buttons_open(buttons_t *b, shared_t *s);   /* 0 ok, -1 error (errno/log) */
/* Bloquea hasta BUTTON_WAIT_NS esperando eventos del kernel y los publica en s. 0 ok, -1 error. */
int  buttons_service(buttons_t *b, shared_t *s);
void buttons_close(buttons_t *b);
#endif
