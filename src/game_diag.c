/* Modo diagnostico: muestra las 2 perillas y los 2 botones en la LCD (Avance 2). */
#include "game.h"
#include <stdio.h>

#define COUNT_MOD 1000u   /* los contadores se muestran con 3 digitos */

static void diag_start(void) { }
static void diag_update(shared_t *s, uint32_t dt_ms) { (void)s; (void)dt_ms; }

static void diag_render(shared_t *s, frame_t *f)
{
    char buf[48];

    /* Fila 0: "P1:1023 P2:1023" valor crudo del ADC */
    snprintf(buf, sizeof(buf), "P1:%4u P2:%4u",
             atomic_load(&s->pot_raw[0]) % 10000u, atomic_load(&s->pot_raw[1]) % 10000u);
    frame_set_row(f, 0, buf);

    /* Fila 1: "B1:#12  B2:-3  " nivel (# presionado, - suelto) y numero de pulsaciones */
    snprintf(buf, sizeof(buf), "B1:%c%-3u B2:%c%-3u",
             atomic_load(&s->btn_level[0]) ? '#' : '-', atomic_load(&s->btn_presses[0]) % COUNT_MOD,
             atomic_load(&s->btn_level[1]) ? '#' : '-', atomic_load(&s->btn_presses[1]) % COUNT_MOD);
    frame_set_row(f, 1, buf);
}

const game_ops_t GAME_DIAG = { "diagnostico", diag_start, diag_update, diag_render };
