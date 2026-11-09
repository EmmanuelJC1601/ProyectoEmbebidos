#ifndef dino_h
#define dino_h
#include "estado.h"
int dino_vivo(void);
int dino_techo(void);
unsigned dino_puntos(void);
/* coloca un cactus encima del juego. solo lo usan las pruebas. */
void forzar_cactus(int x, int alto);
#endif
