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

int botones_abrir(botones_t *b, estado_t *e);
int botones_esperar(botones_t *b, estado_t *e);
void botones_cerrar(botones_t *b);
#endif
