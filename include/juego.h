/* juego.h - menu, dino, pong y prueba. ninguno abre hardware:
 * leen el estado atomico y pintan el framebuffer. */
#ifndef juego_h
#define juego_h
#include "estado.h"

typedef struct {
  const char *nombre;
  void (*iniciar)(estado_t *e);
  void (*actualizar)(estado_t *e);
  void (*dibujar)(estado_t *e);
} juego_t;

/* 1 si actual es un pulso que este juego no habia visto.
 * la primera llamada solo recuerda el conteo, para no repetir el boton que abrio la pantalla. */
int pulso_nuevo(unsigned actual, unsigned *visto, int *listo);
void juegos_iniciar(estado_t *e);
const juego_t *juego_activo(void);
void juego_cambiar(estado_t *e, const juego_t *j);
const juego_t *juego_dino(void);
const juego_t *juego_pong(void);
const juego_t *juego_prueba(void);
const juego_t *juego_menu(void);
#endif
