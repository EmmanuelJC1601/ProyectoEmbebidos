/* botones.h - gpio5 y gpio6 con libgpiod 1.6.
 * bitacora clase 21: el driver es el gpiochip del kernel (pinctrl-bcm2835).
 * no hacemos mmap. el hilo se bloquea en gpiod_line_event_wait_bulk y el kernel
 * lo despierta con la interrupcion del flanco. eso es una syscall por evento.
 * bitacora del debounce: bookworm trae libgpiod 1.6 y ese api no filtra en el kernel.
 * el nivel se publica solo cuando el pin lleva us_debounce (20 ms) sin otro flanco
 * y una lectura lo confirma. un rebote que vuelve al nivel anterior no cuenta como pulso.
 * la latencia medida incluye esos 20 ms y aun asi cabe en el requisito de 50 ms. */
#ifndef botones_h
#define botones_h
#include <stdint.h>
#include "estado.h"

struct gpiod_chip;
struct gpiod_line;

typedef struct {
  struct gpiod_chip *chip;
  struct gpiod_line *linea[n_botones];
  int pedido;
  int pendiente[n_botones];
  int64_t flanco_ns[n_botones];
} botones_t;

int botones_abrir(botones_t *b, estado_t *e);
/* bloquea hasta un flanco, hasta que vence el debounce o hasta ms_espera_boton. */
int botones_esperar(botones_t *b, estado_t *e);
void botones_cerrar(botones_t *b);
#endif
