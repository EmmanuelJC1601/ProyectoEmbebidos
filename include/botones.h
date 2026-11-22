/* botones.h - gpio5 y gpio6.
 * el driver es el gpiochip del kernel. no hacemos mmap: el hilo se bloquea
 * en la espera de flanco y el kernel lo despierta. una syscall por evento.
 * libgpiod 2 (trixie) tampoco filtra rebote. el nivel se publica cuando el
 * pin lleva us_debounce (20 ms) quieto y una lectura lo confirma. un rebote
 * que vuelve al nivel de antes no cuenta como pulso. esos 20 ms van dentro
 * de la latencia y el tope sigue siendo 50 ms. */
#ifndef botones_h
#define botones_h
#include <stdint.h>
#include "estado.h"

struct gpiod_chip;
struct gpiod_line_request;
struct gpiod_edge_event_buffer;

typedef struct {
  struct gpiod_chip *chip;
  struct gpiod_line_request *pedido;
  struct gpiod_edge_event_buffer *eventos;
  int pendiente[n_botones];
  int64_t flanco_ns[n_botones];
} botones_t;

/* abre el chip y pide los dos pines, ambos flancos, pull-down. */
int botones_abrir(botones_t *b, estado_t *e);
/* bloquea hasta un flanco, hasta que vence el debounce o hasta ms_espera_boton. */
int botones_esperar(botones_t *b, estado_t *e);
void botones_cerrar(botones_t *b);
#endif
